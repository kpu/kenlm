/* This is where the trie is built.  It's on-disk.  */
#include "lm/search_trie.hh"

#include "lm/blank.hh"
#include "lm/lm_exception.hh"
#include "lm/read_arpa.hh"
#include "lm/trie.hh"
#include "lm/vocab.hh"
#include "lm/weights.hh"
#include "lm/word_index.hh"
#include "util/ersatz_progress.hh"
#include "util/file_piece.hh"
#include "util/proxy_iterator.hh"
#include "util/scoped.hh"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <deque>
#include <iostream>
#include <limits>
//#include <parallel/algorithm>
#include <vector>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

namespace lm {
namespace ngram {
namespace trie {
namespace {

/* An entry is a n-gram with probability.  It consists of:
 * WordIndex[order]
 * float probability
 * backoff probability (omitted for highest order n-gram)
 * These are stored consecutively in memory.  We want to sort them.  
 *
 * The problem is the length depends on order (but all n-grams being compared
 * have the same order).  Allocating each entry on the heap (i.e. std::vector
 * or std::string) then sorting pointers is the normal solution.  But that's
 * too memory inefficient.  A lot of this code is just here to force std::sort
 * to work with records where length is specified at runtime (and avoid using
 * Boost for LM code).  I could have used qsort, but the point is to also
 * support __gnu_cxx:parallel_sort which doesn't have a qsort version.  
 */

class EntryIterator {
  public:
    EntryIterator() {}

    EntryIterator(void *ptr, std::size_t size) : ptr_(static_cast<uint8_t*>(ptr)), size_(size) {}

    bool operator==(const EntryIterator &other) const {
      return ptr_ == other.ptr_;
    }
    bool operator<(const EntryIterator &other) const {
      return ptr_ < other.ptr_;
    }
    EntryIterator &operator+=(std::ptrdiff_t amount) {
      ptr_ += amount * size_;
      return *this;
    }
    std::ptrdiff_t operator-(const EntryIterator &other) const {
      return (ptr_ - other.ptr_) / size_;
    }

    const void *Data() const { return ptr_; }
    void *Data() { return ptr_; }
    std::size_t EntrySize() const { return size_; }
    
  private:
    uint8_t *ptr_;
    std::size_t size_;
};

class EntryProxy {
  public:
    EntryProxy() {}

    EntryProxy(void *ptr, std::size_t size) : inner_(ptr, size) {}

    operator std::string() const {
      return std::string(reinterpret_cast<const char*>(inner_.Data()), inner_.EntrySize());
    }

    EntryProxy &operator=(const EntryProxy &from) {
      memcpy(inner_.Data(), from.inner_.Data(), inner_.EntrySize());
      return *this;
    }

    EntryProxy &operator=(const std::string &from) {
      memcpy(inner_.Data(), from.data(), inner_.EntrySize());
      return *this;
    }

    const WordIndex *Indices() const {
      return static_cast<const WordIndex*>(inner_.Data());
    }

  private:
    friend class util::ProxyIterator<EntryProxy>;

    typedef std::string value_type;

    typedef EntryIterator InnerIterator;
    InnerIterator &Inner() { return inner_; }
    const InnerIterator &Inner() const { return inner_; } 
    InnerIterator inner_;
};

typedef util::ProxyIterator<EntryProxy> NGramIter;

class CompareRecords : public std::binary_function<const EntryProxy &, const EntryProxy &, bool> {
  public:
    explicit CompareRecords(unsigned char order) : order_(order) {}

    bool operator()(const EntryProxy &first, const EntryProxy &second) const {
      return Compare(first.Indices(), second.Indices());
    }
    bool operator()(const EntryProxy &first, const std::string &second) const {
      return Compare(first.Indices(), reinterpret_cast<const WordIndex*>(second.data()));
    }
    bool operator()(const std::string &first, const EntryProxy &second) const {
      return Compare(reinterpret_cast<const WordIndex*>(first.data()), second.Indices());
    }
    bool operator()(const std::string &first, const std::string &second) const {
      return Compare(reinterpret_cast<const WordIndex*>(first.data()), reinterpret_cast<const WordIndex*>(first.data()));
    }
    
  private:
    bool Compare(const WordIndex *first, const WordIndex *second) const {
      const WordIndex *end = first + order_;
      for (; first != end; ++first, ++second) {
        if (*first < *second) return true;
        if (*first > *second) return false;
      }
      return false;
    }

    unsigned char order_;
};

void WriteOrThrow(FILE *to, const void *data, size_t size) {
  assert(size);
  if (1 != std::fwrite(data, size, 1, to)) UTIL_THROW(util::ErrnoException, "Short write; requested size " << size);
}

void ReadOrThrow(FILE *from, void *data, size_t size) {
  if (1 != std::fread(data, size, 1, from)) UTIL_THROW(util::ErrnoException, "Short read; requested size " << size);
}

const std::size_t kCopyBufSize = 512;
void CopyOrThrow(FILE *from, FILE *to, size_t size) {
  char buf[std::min<size_t>(size, kCopyBufSize)];
  for (size_t i = 0; i < size; i += kCopyBufSize) {
    std::size_t amount = std::min(size - i, kCopyBufSize);
    ReadOrThrow(from, buf, amount);
    WriteOrThrow(to, buf, amount);
  }
}

std::string DiskFlush(const void *mem_begin, const void *mem_end, const std::string &file_prefix, std::size_t batch, unsigned char order, std::size_t weights_size) {
  const std::size_t entry_size = sizeof(WordIndex) * order + weights_size;
  const std::size_t prefix_size = sizeof(WordIndex) * (order - 1);
  std::stringstream assembled;
  assembled << file_prefix << static_cast<unsigned int>(order) << '_' << batch;
  std::string ret(assembled.str());
  util::scoped_FILE out(fopen(ret.c_str(), "w"));
  if (!out.get()) UTIL_THROW(util::ErrnoException, "Couldn't open " << assembled.str().c_str() << " for writing");
  // Compress entries that being with the same (order-1) words.
  for (const uint8_t *group_begin = static_cast<const uint8_t*>(mem_begin); group_begin != static_cast<const uint8_t*>(mem_end);) {
    const uint8_t *group_end;
    for (group_end = group_begin + entry_size;
         (group_end != static_cast<const uint8_t*>(mem_end)) && !memcmp(group_begin, group_end, prefix_size);
         group_end += entry_size) {}
    WriteOrThrow(out.get(), group_begin, prefix_size);
    WordIndex group_size = (group_end - group_begin) / entry_size;
    WriteOrThrow(out.get(), &group_size, sizeof(group_size));
    for (const uint8_t *i = group_begin; i != group_end; i += entry_size) {
      WriteOrThrow(out.get(), i + prefix_size, sizeof(WordIndex));
      WriteOrThrow(out.get(), i + sizeof(WordIndex) * order, weights_size);
    }
    group_begin = group_end;
  }
  return ret;
}

class SortedFileReader {
  public:
    SortedFileReader() : ended_(false) {}

    void Init(const std::string &name, unsigned char order) {
      file_.reset(fopen(name.c_str(), "r"));
      if (!file_.get()) UTIL_THROW(util::ErrnoException, "Opening " << name << " for read");
      header_.resize(order - 1);
      NextHeader();
    }

    // Preceding words.
    const WordIndex *Header() const {
      return &*header_.begin();
    }
    const std::vector<WordIndex> &HeaderVector() const { return header_;} 

    std::size_t HeaderBytes() const { return header_.size() * sizeof(WordIndex); }

    void NextHeader() {
      if (1 != fread(&*header_.begin(), HeaderBytes(), 1, file_.get())) {
        if (feof(file_.get())) {
          ended_ = true;
        } else {
          UTIL_THROW(util::ErrnoException, "Short read of counts");
        }
      }
    }

    WordIndex ReadCount() {
      WordIndex ret;
      ReadOrThrow(file_.get(), &ret, sizeof(WordIndex));
      return ret;
    }

    WordIndex ReadWord() {
      WordIndex ret;
      ReadOrThrow(file_.get(), &ret, sizeof(WordIndex));
      return ret;
    }

    template <class Weights> void ReadWeights(Weights &weights) {
      ReadOrThrow(file_.get(), &weights, sizeof(Weights));
    }

    bool Ended() {
      return ended_;
    }

    void Rewind() {
      rewind(file_.get());
      ended_ = false;
      NextHeader();
    }

    FILE *File() { return file_.get(); }

  private:
    util::scoped_FILE file_;

    std::vector<WordIndex> header_;

    bool ended_;
};

void CopyFullRecord(SortedFileReader &from, FILE *to, std::size_t weights_size) {
  WriteOrThrow(to, from.Header(), from.HeaderBytes());
  WordIndex count = from.ReadCount();
  WriteOrThrow(to, &count, sizeof(WordIndex));

  CopyOrThrow(from.File(), to, (weights_size + sizeof(WordIndex)) * count);
}

void MergeSortedFiles(const char *first_name, const char *second_name, const char *out, std::size_t weights_size, unsigned char order) {
  SortedFileReader first, second;
  first.Init(first_name, order);
  second.Init(second_name, order);
  util::scoped_FILE out_file(fopen(out, "w"));
  if (!out_file.get()) UTIL_THROW(util::ErrnoException, "Could not open " << out << " for write");
  while (!first.Ended() && !second.Ended()) {
    if (first.HeaderVector() < second.HeaderVector()) {
      CopyFullRecord(first, out_file.get(), weights_size);
      first.NextHeader();
      continue;
    } 
    if (first.HeaderVector() > second.HeaderVector()) {
      CopyFullRecord(second, out_file.get(), weights_size);
      second.NextHeader();
      continue;
    }
    // Merge at the entry level.
    WriteOrThrow(out_file.get(), first.Header(), first.HeaderBytes());
    WordIndex first_count = first.ReadCount(), second_count = second.ReadCount();
    WordIndex total_count = first_count + second_count;
    WriteOrThrow(out_file.get(), &total_count, sizeof(WordIndex));

    WordIndex first_word = first.ReadWord(), second_word = second.ReadWord();
    WordIndex first_index = 0, second_index = 0;
    while (true) {
      if (first_word < second_word) {
        WriteOrThrow(out_file.get(), &first_word, sizeof(WordIndex));
        CopyOrThrow(first.File(), out_file.get(), weights_size);
        if (++first_index == first_count) break;
        first_word = first.ReadWord();
      } else {
        WriteOrThrow(out_file.get(), &second_word, sizeof(WordIndex));
        CopyOrThrow(second.File(), out_file.get(), weights_size);
        if (++second_index == second_count) break;
        second_word = second.ReadWord();
      }
    }
    if (first_index == first_count) {
      WriteOrThrow(out_file.get(), &second_word, sizeof(WordIndex));
      CopyOrThrow(second.File(), out_file.get(), (second_count - second_index) * (weights_size + sizeof(WordIndex)) - sizeof(WordIndex));
    } else {
      WriteOrThrow(out_file.get(), &first_word, sizeof(WordIndex));
      CopyOrThrow(first.File(), out_file.get(), (first_count - first_index) * (weights_size + sizeof(WordIndex)) - sizeof(WordIndex));
    }
    first.NextHeader();
    second.NextHeader();
  }

  for (SortedFileReader &remaining = first.Ended() ? second : first; !remaining.Ended(); remaining.NextHeader()) {
    CopyFullRecord(remaining, out_file.get(), weights_size);
  }
}

void ConvertToSorted(util::FilePiece &f, const SortedVocabulary &vocab, const std::vector<uint64_t> &counts, util::scoped_memory &mem, const std::string &file_prefix, unsigned char order) {
  if (order == 1) return;
  ConvertToSorted(f, vocab, counts, mem, file_prefix, order - 1);

  ReadNGramHeader(f, order);
  const size_t count = counts[order - 1];
  // Size of weights.  Does it include backoff?  
  const size_t words_size = sizeof(WordIndex) * order;
  const size_t weights_size = sizeof(float) + ((order == counts.size()) ? 0 : sizeof(float));
  const size_t entry_size = words_size + weights_size;
  const size_t batch_size = std::min(count, mem.size() / entry_size);
  uint8_t *const begin = reinterpret_cast<uint8_t*>(mem.get());
  std::deque<std::string> files;
  for (std::size_t batch = 0, done = 0; done < count; ++batch) {
    uint8_t *out = begin;
    uint8_t *out_end = out + std::min(count - done, batch_size) * entry_size;
    if (order == counts.size()) {
      for (; out != out_end; out += entry_size) {
        ReadNGram(f, order, vocab, reinterpret_cast<WordIndex*>(out), *reinterpret_cast<Prob*>(out + words_size));
      }
    } else {
      for (; out != out_end; out += entry_size) {
        ReadNGram(f, order, vocab, reinterpret_cast<WordIndex*>(out), *reinterpret_cast<ProbBackoff*>(out + words_size));
      }
    }
    // TODO: __gnu_parallel::sort here.
    EntryProxy proxy_begin(begin, entry_size), proxy_end(out_end, entry_size);
    std::sort(NGramIter(proxy_begin), NGramIter(proxy_end), CompareRecords(order));
    
    files.push_back(DiskFlush(begin, out_end, file_prefix, batch, order, weights_size));
    done += (out_end - begin) / entry_size;
  }

  // All individual files created.  Merge them.  

  std::size_t merge_count = 0;
  while (files.size() > 1) {
    std::stringstream assembled;
    assembled << file_prefix << static_cast<unsigned int>(order) << "_merge_" << (merge_count++);
    files.push_back(assembled.str());
    MergeSortedFiles(files[0].c_str(), files[1].c_str(), files.back().c_str(), weights_size, order);
    if (std::remove(files[0].c_str())) UTIL_THROW(util::ErrnoException, "Could not remove " << files[0]);
    files.pop_front();
    if (std::remove(files[0].c_str())) UTIL_THROW(util::ErrnoException, "Could not remove " << files[0]);
    files.pop_front();
  }
  if (!files.empty()) {
    std::stringstream assembled;
    assembled << file_prefix << static_cast<unsigned int>(order) << "_merged";
    std::string merged_name(assembled.str());
    if (std::rename(files[0].c_str(), merged_name.c_str())) UTIL_THROW(util::ErrnoException, "Could not rename " << files[0].c_str() << " to " << merged_name.c_str());
  }
}

void ARPAToSortedFiles(util::FilePiece &f, const std::vector<uint64_t> &counts, std::size_t buffer, const std::string &file_prefix, SortedVocabulary &vocab) {
  {
    std::string unigram_name = file_prefix + "unigrams";
    util::scoped_fd unigram_file;
    util::scoped_mmap unigram_mmap(util::MapZeroedWrite(unigram_name.c_str(), counts[0] * sizeof(ProbBackoff), unigram_file), counts[0] * sizeof(ProbBackoff));
    Read1Grams(f, counts[0], vocab, reinterpret_cast<ProbBackoff*>(unigram_mmap.get()));
  }

  util::scoped_memory mem;
  mem.reset(malloc(buffer), buffer, util::scoped_memory::MALLOC_ALLOCATED);
  if (!mem.get()) UTIL_THROW(util::ErrnoException, "malloc failed for sort buffer size " << buffer);
  ConvertToSorted(f, vocab, counts, mem, file_prefix, counts.size());
  ReadEnd(f);
}

bool HeadMatch(const WordIndex *words, const WordIndex *const words_end, const WordIndex *header) {
  for (; words != words_end; ++words, ++header) {
    if (*words != *header) {
      assert(*words <= *header);
      return false;
    }
  }
  return true;
}

class JustCount {
  public:
    JustCount(UnigramValue * /*unigrams*/, BitPackedMiddle * /*middle*/, BitPackedLongest &/*longest*/, uint64_t *counts, unsigned char order)
      : counts_(counts), longest_counts_(counts + order - 1) {}

    void Unigrams(WordIndex begin, WordIndex end) {
      counts_[0] += end - begin;
    }

    void MiddleBlank(const unsigned char mid_idx, WordIndex /* idx */) {
      ++counts_[mid_idx + 1];
    }

    void Middle(const unsigned char mid_idx, WordIndex /*key*/, const ProbBackoff &/*weights*/) {
      ++counts_[mid_idx + 1];
    }

    void Longest(WordIndex /*key*/, Prob /*prob*/) {
      ++*longest_counts_;
    }

    // Unigrams wrote one past.  
    void Cleanup() {
      --counts_[0];
    }

  private:
    uint64_t *const counts_, *const longest_counts_;
};

class WriteEntries {
  public:
    WriteEntries(UnigramValue *unigrams, BitPackedMiddle *middle, BitPackedLongest &longest, const uint64_t * /*counts*/, unsigned char order) : 
      unigrams_(unigrams),
      middle_(middle),
      longest_(longest), 
      bigram_pack_((order == 2) ? static_cast<BitPacked&>(longest_) : static_cast<BitPacked&>(*middle_)) {}

    void Unigrams(WordIndex begin, WordIndex end) {
      uint64_t next = bigram_pack_.InsertIndex();
      for (UnigramValue *i = unigrams_ + begin; i < unigrams_ + end; ++i) {
        i->next = next;
      }
    }

    void MiddleBlank(const unsigned char mid_idx, WordIndex key) {
      middle_[mid_idx].Insert(key, kBlankProb, kBlankBackoff);
    }

    void Middle(const unsigned char mid_idx, WordIndex key, const ProbBackoff &weights) {
      middle_[mid_idx].Insert(key, weights.prob, weights.backoff);
    }

    void Longest(WordIndex key, Prob prob) {
      longest_.Insert(key, prob.prob);
    }

    void Cleanup() {}

  private:
    UnigramValue *const unigrams_;
    BitPackedMiddle *const middle_;
    BitPackedLongest &longest_;
    BitPacked &bigram_pack_;
};

template <class Doing> class RecursiveInsert {
  public:
    RecursiveInsert(SortedFileReader *inputs, UnigramValue *unigrams, BitPackedMiddle *middle, BitPackedLongest &longest, uint64_t *counts, unsigned char order) : 
      doing_(unigrams, middle, longest, counts, order), inputs_(inputs), inputs_end_(inputs + order - 1), words_(new WordIndex[order]), order_minus_2_(order - 2) {
    }

    // Outer unigram loop.
    void Apply(std::ostream *progress_out, const char *message, WordIndex unigram_count) {
      util::ErsatzProgress progress(progress_out, message, unigram_count + 1);
      for (words_[0] = 0; ; ++words_[0]) {
        WordIndex min_continue = unigram_count;
        for (SortedFileReader *other = inputs_; other != inputs_end_; ++other) {
          if (other->Ended()) continue;
          min_continue = std::min(min_continue, other->Header()[0]);
        }
        // This will write at unigram_count.  This is by design so that the next pointers will make sense.  
        doing_.Unigrams(words_[0], min_continue + 1);
        if (min_continue == unigram_count) break;
        progress += min_continue - words_[0];
        words_[0] = min_continue;
        Middle(0);
      }
      doing_.Cleanup();
    }

  private:
    void Middle(const unsigned char mid_idx) {
      // (mid_idx + 2)-gram.
      if (mid_idx == order_minus_2_) {
        Longest();
        return;
      }
      // Orders [2, order)

      SortedFileReader &reader = inputs_[mid_idx];

      if (reader.Ended() || !HeadMatch(words_.get(), words_.get() + mid_idx + 1, reader.Header())) {
        // This order doesn't have a header match, but longer ones might.  
        MiddleAllBlank(mid_idx);
        return;
      }

      // There is a header match.  
      WordIndex count = reader.ReadCount();
      WordIndex current = reader.ReadWord();
      while (count) {
        WordIndex min_continue = std::numeric_limits<WordIndex>::max();
        for (SortedFileReader *other = inputs_ + mid_idx + 1; other < inputs_end_; ++other) {
          if (!other->Ended() && HeadMatch(words_.get(), words_.get() + mid_idx + 1, other->Header()))
            min_continue = std::min(min_continue, other->Header()[mid_idx + 1]);
        }
        while (true) {
          if (current > min_continue) {
            doing_.MiddleBlank(mid_idx, min_continue);
            words_[mid_idx + 1] = min_continue;
            Middle(mid_idx + 1);
            break;
          }
          ProbBackoff weights;
          reader.ReadWeights(weights);
          doing_.Middle(mid_idx, current, weights);
          --count;
          if (current == min_continue) {
            words_[mid_idx + 1] = min_continue;
            Middle(mid_idx + 1);
            if (count) current = reader.ReadWord();
            break;
          }
          if (!count) break;
          current = reader.ReadWord();
        }
      }
      // Count is now zero.  Finish off remaining blanks.  
      MiddleAllBlank(mid_idx);
      reader.NextHeader();
    }

    void MiddleAllBlank(const unsigned char mid_idx) {
      while (true) {
        WordIndex min_continue = std::numeric_limits<WordIndex>::max();
        for (SortedFileReader *other = inputs_ + mid_idx + 1; other < inputs_end_; ++other) {
          if (!other->Ended() && HeadMatch(words_.get(), words_.get() + mid_idx + 1, other->Header()))
            min_continue = std::min(min_continue, other->Header()[mid_idx + 1]);
        }
        if (min_continue == std::numeric_limits<WordIndex>::max()) return;
        doing_.MiddleBlank(mid_idx, min_continue);
        words_[mid_idx + 1] = min_continue;
        Middle(mid_idx + 1);
      }
    }

    void Longest() {
      SortedFileReader &reader = *(inputs_end_ - 1);
      if (reader.Ended() || !HeadMatch(words_.get(), words_.get() + order_minus_2_ + 1, reader.Header())) return;
      WordIndex count = reader.ReadCount();
      for (WordIndex i = 0; i < count; ++i) {
        WordIndex word = reader.ReadWord();
        Prob prob;
        reader.ReadWeights(prob);
        doing_.Longest(word, prob);
      }
      reader.NextHeader();
      return;
    }

    Doing doing_;

    SortedFileReader *inputs_;
    SortedFileReader *inputs_end_;

    util::scoped_array<WordIndex> words_;

    const unsigned char order_minus_2_;
};

void SanityCheckCounts(const std::vector<uint64_t> &initial, const std::vector<uint64_t> &fixed) {
  if (fixed[0] != initial[0]) UTIL_THROW(util::Exception, "Unigram count should be constant but initial is " << initial[0] << " and recounted is " << fixed[0]);
  if (fixed.back() != initial.back()) UTIL_THROW(util::Exception, "Longest count should be constant");
  for (unsigned char i = 0; i < initial.size(); ++i) {
    if (fixed[i] < initial[i]) UTIL_THROW(util::Exception, "Counts came out lower than expected.  This shouldn't happen");
  }
}

void BuildTrie(const std::string &file_prefix, const std::vector<uint64_t> &counts, const Config &config, TrieSearch &out, Backing &backing) {
  SortedFileReader inputs[counts.size() - 1];

  for (unsigned char i = 2; i <= counts.size(); ++i) {
    std::stringstream assembled;
    assembled << file_prefix << static_cast<unsigned int>(i) << "_merged";
    inputs[i-2].Init(assembled.str(), i);
    unlink(assembled.str().c_str());
  }

  std::vector<uint64_t> fixed_counts(counts.size());
  {
    RecursiveInsert<JustCount> counter(inputs, NULL, &*out.middle.begin(), out.longest, &*fixed_counts.begin(), counts.size());
    counter.Apply(config.messages, "Counting n-grams that should not have been pruned", counts[0]);
  }
  SanityCheckCounts(counts, fixed_counts);

  out.SetupMemory(GrowForSearch(config, TrieSearch::kModelType, fixed_counts, TrieSearch::Size(fixed_counts, config), backing), fixed_counts, config);

  for (unsigned char i = 2; i <= counts.size(); ++i) {
    inputs[i-2].Rewind();
  }
  UnigramValue *unigrams = out.unigram.Raw();
  // Fill entries except unigram probabilities.  
  {
    RecursiveInsert<WriteEntries> inserter(inputs, unigrams, &*out.middle.begin(), out.longest, &*fixed_counts.begin(), counts.size());
    inserter.Apply(config.messages, "Building trie", fixed_counts[0]);
  }

  // Fill unigram probabilities.  
  {
    std::string name(file_prefix + "unigrams");
    util::scoped_FILE file(fopen(name.c_str(), "r"));
    if (!file.get()) UTIL_THROW(util::ErrnoException, "Opening " << name << " failed");
    for (WordIndex i = 0; i < counts[0]; ++i) {
      ReadOrThrow(file.get(), &unigrams[i].weights, sizeof(ProbBackoff));
    }
    unlink(name.c_str());
  }

  /* Set ending offsets so the last entry will be sized properly */
  // Last entry for unigrams was already set.  
  if (!out.middle.empty()) {
    for (size_t i = 0; i < out.middle.size() - 1; ++i) {
      out.middle[i].FinishedLoading(out.middle[i+1].InsertIndex());
    }
    out.middle.back().FinishedLoading(out.longest.InsertIndex());
  }
}

} // namespace

void TrieSearch::InitializeFromARPA(const char *file, util::FilePiece &f, std::vector<uint64_t> &counts, const Config &config, SortedVocabulary &vocab, Backing &backing) {
  std::string temporary_directory;
  if (config.temporary_directory_prefix) {
    temporary_directory = config.temporary_directory_prefix;
  } else if (config.write_mmap) {
    temporary_directory = config.write_mmap;
  } else {
    temporary_directory = file;
  }
  // Null on end is kludge to ensure null termination.
  temporary_directory += "-tmp-XXXXXX";
  temporary_directory += '\0';
  if (!mkdtemp(&temporary_directory[0])) {
    UTIL_THROW(util::ErrnoException, "Failed to make a temporary directory based on the name " << temporary_directory.c_str());
  }
  // Chop off null kludge.  
  temporary_directory.resize(strlen(temporary_directory.c_str()));
  // Add directory delimiter.  Assumes a real operating system.  
  temporary_directory += '/';
  // At least 1MB sorting memory.  
  ARPAToSortedFiles(f, counts, std::max<size_t>(config.building_memory, 1048576), temporary_directory.c_str(), vocab);

  BuildTrie(temporary_directory.c_str(), counts, config, *this, backing);
  if (rmdir(temporary_directory.c_str())) {
    std::cerr << "Failed to delete " << temporary_directory << std::endl;
  }
}

} // namespace trie
} // namespace ngram
} // namespace lm

/* This is where the trie is built.  It's on-disk.  */
#include "lm/search_trie.hh"

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

void CopyOrThrow(FILE *from, FILE *to, size_t size) {
  const size_t kBufSize = 512;
  char buf[kBufSize];
  for (size_t i = 0; i < size; i += kBufSize) {
    std::size_t amount = std::min(size - i, kBufSize);
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
    SortedFileReader() {}

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
      if (1 != fread(&*header_.begin(), HeaderBytes(), 1, file_.get()) && !Ended()) {
        UTIL_THROW(util::ErrnoException, "Short read of counts");
      }
    }

    void ReadCount(WordIndex &to) {
      ReadOrThrow(file_.get(), &to, sizeof(WordIndex));
    }

    void ReadWord(WordIndex &to) {
      ReadOrThrow(file_.get(), &to, sizeof(WordIndex));
    }

    template <class Weights> void ReadWeights(Weights &to) {
      ReadOrThrow(file_.get(), &to, sizeof(Weights));
    }

    template <class Weights> void SkipEntry() {
      WordIndex count;
      ReadCount(count);
      if (fseek(file_.get(), count * (sizeof(Weights) + sizeof(WordIndex)), SEEK_CUR))
        UTIL_THROW(util::ErrnoException, "Failed to seek past an entry.");
      NextHeader();
    }

    bool Ended() {
      return feof(file_.get());
    }

    FILE *File() { return file_.get(); }

  private:
    util::scoped_FILE file_;

    std::vector<WordIndex> header_;
};

void CopyFullRecord(SortedFileReader &from, FILE *to, std::size_t weights_size) {
  WriteOrThrow(to, from.Header(), from.HeaderBytes());
  WordIndex count;
  from.ReadCount(count);
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
    WordIndex first_count, second_count;
    first.ReadCount(first_count); second.ReadCount(second_count);
    WordIndex total_count = first_count + second_count;
    WriteOrThrow(out_file.get(), &total_count, sizeof(WordIndex));

    WordIndex first_word, second_word;
    first.ReadWord(first_word); second.ReadWord(second_word);
    WordIndex first_index = 0, second_index = 0;
    while (true) {
      if (first_word < second_word) {
        WriteOrThrow(out_file.get(), &first_word, sizeof(WordIndex));
        CopyOrThrow(first.File(), out_file.get(), weights_size);
        if (++first_index == first_count) break;
        first.ReadWord(first_word);
      } else {
        WriteOrThrow(out_file.get(), &second_word, sizeof(WordIndex));
        CopyOrThrow(second.File(), out_file.get(), weights_size);
        if (++second_index == second_count) break;
        second.ReadWord(second_word);
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
  mem.reset(malloc(buffer), buffer, util::scoped_memory::ARRAY_ALLOCATED);
  if (!mem.get()) UTIL_THROW(util::ErrnoException, "malloc failed for sort buffer size " << buffer);
  ConvertToSorted(f, vocab, counts, mem, file_prefix, counts.size());
  ReadEnd(f);
}

struct RecursiveInsertParams {
  WordIndex *words;
  SortedFileReader *files;
  unsigned char max_order;
  // This is an array of size order - 2.
  BitPackedMiddle *middle;
  // This has exactly one entry.
  BitPackedLongest *longest;
};

uint64_t RecursiveInsert(RecursiveInsertParams &params, unsigned char order) {
  SortedFileReader &file = params.files[order - 2];
  const uint64_t ret = (order == params.max_order) ? params.longest->InsertIndex() : params.middle[order - 2].InsertIndex();

  while (true) {
    if (file.Ended()) return ret;
    bool param_is_greater = false;
    const WordIndex *param = static_cast<const WordIndex*>(params.words);
    const WordIndex *head = static_cast<const WordIndex*>(file.Header());
    for (; param != static_cast<const WordIndex*>(params.words) + order - 1; ++param, ++head) {
      if (*param < *head) return ret;
      if (*param > *head) {
        param_is_greater = true;
        break;
      }
    }
    if (!param_is_greater) break;

    // TODO: better than skipping entry
    if (order == params.max_order) {
      file.SkipEntry<Prob>();
    } else {
      file.SkipEntry<ProbBackoff>();
    }
  }

  WordIndex count;
  file.ReadCount(count);
  WordIndex key;
  if (order == params.max_order) {
    Prob value;
    for (WordIndex i = 0; i < count; ++i) {
      file.ReadWord(key);
      file.ReadWeights(value);
      params.longest->Insert(key, value.prob);
    }
    file.NextHeader();
    return ret;
  }
  ProbBackoff value;
  for (WordIndex i = 0; i < count; ++i) {
    file.ReadWord(params.words[order - 1]);
    file.ReadWeights(value);
    params.middle[order - 2].Insert(
        params.words[order - 1],
        value.prob,
        value.backoff,
        RecursiveInsert(params, order + 1));
  }
  file.NextHeader();
  return ret;
}

void BuildTrie(const std::string &file_prefix, const std::vector<uint64_t> &counts, std::ostream *messages, TrieSearch &out) {
  UnigramValue *unigrams = out.unigram.Raw();
  // Load unigrams.  Leave the next pointers uninitialized.   
  {
    std::string name(file_prefix + "unigrams");
    util::scoped_FILE file(fopen(name.c_str(), "r"));
    if (!file.get()) UTIL_THROW(util::ErrnoException, "Opening " << name << " failed");
    for (WordIndex i = 0; i < counts[0]; ++i) {
      ReadOrThrow(file.get(), &unigrams[i].weights, sizeof(ProbBackoff));
    }
    unlink(name.c_str());
  }

  // inputs[0] is bigrams.
  SortedFileReader inputs[counts.size() - 1];
  for (unsigned char i = 2; i <= counts.size(); ++i) {
    std::stringstream assembled;
    assembled << file_prefix << static_cast<unsigned int>(i) << "_merged";
    inputs[i-2].Init(assembled.str(), i);
    unlink(assembled.str().c_str());
  }

  // words[0] is unigrams.  
  WordIndex words[counts.size()];
  RecursiveInsertParams params;
  params.words = words;
  params.files = inputs;
  params.max_order = static_cast<unsigned char>(counts.size());
  params.middle = &*out.middle.begin();
  params.longest = &out.longest;
  {
    util::ErsatzProgress progress(messages, "Building trie", counts[0]);
    for (words[0] = 0; words[0] < counts[0]; ++words[0], ++progress) {
      unigrams[words[0]].next = RecursiveInsert(params, 2);
    }
  }

  /* Set ending offsets so the last entry will be sized properly */
  if (!out.middle.empty()) {
    unigrams[counts[0]].next = out.middle.front().InsertIndex();
    for (size_t i = 0; i < out.middle.size() - 1; ++i) {
      out.middle[i].FinishedLoading(out.middle[i+1].InsertIndex());
    }
    out.middle.back().FinishedLoading(out.longest.InsertIndex());
  } else {
    unigrams[counts[0]].next = out.longest.InsertIndex();
  }
}

} // namespace

void TrieSearch::InitializeFromARPA(const char *file, util::FilePiece &f, const std::vector<uint64_t> &counts, const Config &config, SortedVocabulary &vocab, Backing &backing) {
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

  SetupMemory(GrowForSearch(config, kModelType, counts, Size(counts, config), backing), counts, config);
  BuildTrie(temporary_directory.c_str(), counts, config.messages, *this);
  if (rmdir(temporary_directory.c_str())) {
    std::cerr << "Failed to delete " << temporary_directory << std::endl;
  }
}

} // namespace trie
} // namespace ngram
} // namespace lm

#include "lm/search_trie.hh"

#include "lm/lm_exception.hh"
#include "lm/read_arpa.hh"
#include "lm/trie.hh"
#include "lm/vocab.hh"
#include "lm/weights.hh"
#include "lm/word_index.hh"
#include "util/ersatz_progress.hh"
#include "util/file_piece.hh"
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

template <unsigned char Order> class FullEntry {
  public:
    typedef ProbBackoff Weights;
    static const unsigned char kOrder = Order;

    // reverse order
    WordIndex words[Order];
    Weights weights;

    bool operator<(const FullEntry<Order> &other) const {
      for (const WordIndex *i = words, *j = other.words; i != words + Order; ++i, ++j) {
        if (*i < *j) return true;
        if (*i > *j) return false;
      }
      return false;
    }
};

template <unsigned char Order> class ProbEntry {
  public:
    typedef Prob Weights;
    static const unsigned char kOrder = Order;

    // reverse order
    WordIndex words[Order];
    Weights weights;

    bool operator<(const ProbEntry<Order> &other) const {
      for (const WordIndex *i = words, *j = other.words; i != words + Order; ++i, ++j) {
        if (*i < *j) return true;
        if (*i > *j) return false;
      }
      return false;
    }
};

void WriteOrThrow(FILE *to, const void *data, size_t size) {
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

template <class Entry> std::string DiskFlush(const Entry *begin, const Entry *end, const std::string &file_prefix, std::size_t batch) {
  std::stringstream assembled;
  assembled << file_prefix << static_cast<unsigned int>(Entry::kOrder) << '_' << batch;
  std::string ret(assembled.str());
  util::scoped_FILE out(fopen(ret.c_str(), "w"));
  if (!out.get()) UTIL_THROW(util::ErrnoException, "Couldn't open " << assembled.str().c_str() << " for writing");
  for (const Entry *group_begin = begin; group_begin != end;) {
    const Entry *group_end = group_begin;
    for (++group_end; (group_end != end) && !memcmp(group_begin->words, group_end->words, sizeof(WordIndex) * (Entry::kOrder - 1)); ++group_end) {}
    WriteOrThrow(out.get(), group_begin->words, sizeof(WordIndex) * (Entry::kOrder - 1));
    WordIndex group_size = group_end - group_begin;
    WriteOrThrow(out.get(), &group_size, sizeof(group_size));
    for (const Entry *i = group_begin; i != group_end; ++i) {
      WriteOrThrow(out.get(), &i->words[Entry::kOrder - 1], sizeof(WordIndex));
      WriteOrThrow(out.get(), &i->weights, sizeof(typename Entry::Weights));
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

template <class Entry> void ConvertToSorted(util::FilePiece &f, const SortedVocabulary &vocab, const std::vector<uint64_t> &counts, util::scoped_memory &mem, const std::string &file_prefix) {
  ConvertToSorted<FullEntry<Entry::kOrder - 1> >(f, vocab, counts, mem, file_prefix);

  ReadNGramHeader(f, Entry::kOrder);
  const size_t count = counts[Entry::kOrder - 1];
  const size_t batch_size = std::min(count, mem.size() / sizeof(Entry));
  Entry *const begin = reinterpret_cast<Entry*>(mem.get());
  std::deque<std::string> files;
  for (std::size_t batch = 0, done = 0; done < count; ++batch) {
    Entry *out = begin;
    Entry *out_end = out + std::min(count - done, batch_size);
    for (; out != out_end; ++out) {
      ReadNGram(f, Entry::kOrder, vocab, out->words, out->weights);
    }
    //__gnu_parallel::sort(begin, out_end);
    std::sort(begin, out_end);
    
    files.push_back(DiskFlush(begin, out_end, file_prefix, batch));
    done += out_end - begin;
  }

  // All individual files created.  Merge them.  

  std::size_t merge_count = 0;
  while (files.size() > 1) {
    std::stringstream assembled;
    assembled << file_prefix << static_cast<unsigned int>(Entry::kOrder) << "_merge_" << (merge_count++);
    files.push_back(assembled.str());
    MergeSortedFiles(files[0].c_str(), files[1].c_str(), files.back().c_str(), sizeof(typename Entry::Weights), Entry::kOrder);
    if (std::remove(files[0].c_str())) UTIL_THROW(util::ErrnoException, "Could not remove " << files[0]);
    files.pop_front();
    if (std::remove(files[0].c_str())) UTIL_THROW(util::ErrnoException, "Could not remove " << files[0]);
    files.pop_front();
  }
  if (!files.empty()) {
    std::stringstream assembled;
    assembled << file_prefix << static_cast<unsigned int>(Entry::kOrder) << "_merged";
    std::string merged_name(assembled.str());
    if (std::rename(files[0].c_str(), merged_name.c_str())) UTIL_THROW(util::ErrnoException, "Could not rename " << files[0].c_str() << " to " << merged_name.c_str());
  }
}

template <> void ConvertToSorted<FullEntry<1> >(util::FilePiece &/*f*/, const SortedVocabulary &/*vocab*/, const std::vector<uint64_t> &/*counts*/, util::scoped_memory &/*mem*/, const std::string &/*file_prefix*/) {}

void ARPAToSortedFiles(util::FilePiece &f, const std::vector<uint64_t> &counts, std::size_t buffer, const std::string &file_prefix, SortedVocabulary &vocab) {
  {
    std::string unigram_name = file_prefix + "unigrams";
    util::scoped_mapped_file unigram_file;
    util::MapZeroedWrite(unigram_name.c_str(), counts[0] * sizeof(ProbBackoff), unigram_file);
    Read1Grams(f, counts[0], vocab, reinterpret_cast<ProbBackoff*>(unigram_file.mem.get()));
  }

  util::scoped_memory mem;
  mem.reset(new char[buffer], buffer, util::scoped_memory::ARRAY_ALLOCATED);
  ConvertToSorted<ProbEntry<5> >(f, vocab, counts, mem, file_prefix);
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
  if (std::memcmp(params.words, file.Header(), sizeof(WordIndex) * (order - 1)))
    return ret;
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

void TrieSearch::InitializeFromARPA(const char *file, util::FilePiece &f, const std::vector<uint64_t> &counts, const Config &config, SortedVocabulary &vocab) {
  std::string temporary_directory;
  if (config.temporary_directory_prefix) {
    temporary_directory = config.temporary_directory_prefix;
  } else if (config.write_mmap) {
    temporary_directory = config.write_mmap;
  } else {
    temporary_directory = file;
  }
  // Null on end is kludge to ensure null termination.
  temporary_directory += "-tmp-XXXXXX\0";
  if (!mkdtemp(&temporary_directory[0])) {
    UTIL_THROW(util::ErrnoException, "Failed to make a temporary directory based on the name " << temporary_directory.c_str());
  }
  // Chop off null kludge.  
  temporary_directory.resize(strlen(temporary_directory.c_str()));
  // Add directory delimiter.  Assumes a real operating system.  
  temporary_directory += '/';
  ARPAToSortedFiles(f, counts, config.building_memory, temporary_directory.c_str(), vocab);
  BuildTrie(temporary_directory.c_str(), counts, config.messages, *this);
  if (rmdir(temporary_directory.c_str())) {
    std::cerr << "Failed to delete " << temporary_directory << std::endl;
  }
}

} // namespace trie
} // namespace ngram
} // namespace lm

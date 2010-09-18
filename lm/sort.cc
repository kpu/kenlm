#include "lm/exception.hh"
#include "lm/ngram.hh"
#include "lm/word_index.hh"
#include "util/file_piece.hh"
#include "util/scoped.hh"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <limits>
//#include <parallel/algorithm>
#include <vector>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace lm {

using ngram::detail::Prob;
using ngram::detail::ProbBackoff;


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

bool IsEntirelyWhiteSpace(const StringPiece &line) {
  for (size_t i = 0; i < static_cast<size_t>(line.size()); ++i) {
    if (!isspace(line.data()[i])) return false;
  }
  return true;
}

void ReadARPACounts(util::FilePiece &in, std::vector<size_t> &number) {
  number.clear();
  StringPiece line;
  if (!IsEntirelyWhiteSpace(line = in.ReadLine())) UTIL_THROW(FormatLoadException, "First line was \"" << line << "\" not blank");
  if ((line = in.ReadLine()) != "\\data\\") UTIL_THROW(FormatLoadException, "second line was \"" << line << "\" not \\data\\.");
  while (!IsEntirelyWhiteSpace(line = in.ReadLine())) {
    if (line.size() < 6 || strncmp(line.data(), "ngram ", 6)) UTIL_THROW(FormatLoadException, "count line \"" << line << "\"doesn't begin with       \"ngram \"");
    // So strtol doesn't go off the end of line.  
    std::string remaining(line.data() + 6, line.size() - 6);
    char *end_ptr;
    unsigned long int length = std::strtol(remaining.c_str(), &end_ptr, 10);
    if ((end_ptr == remaining.c_str()) || (length - 1 != number.size())) UTIL_THROW(FormatLoadException, "ngram count lengths should be consecutive  starting with 1: " << line);
    if (*end_ptr != '=') UTIL_THROW(FormatLoadException, "Expected = immediately following the first number in the count line " << line);
    ++end_ptr;
    const char *start = end_ptr;
    long int count = std::strtol(start, &end_ptr, 10);
    if (count < 0) UTIL_THROW(FormatLoadException, "Negative n-gram count " << count);
    if (start == end_ptr) UTIL_THROW(FormatLoadException, "Couldn't parse n-gram count from " << line);
    number.push_back(count);
  }
}

void ReadNGramHeader(util::FilePiece &in, unsigned int length) {
  StringPiece line;
  while (IsEntirelyWhiteSpace(line = in.ReadLine())) {}
  std::stringstream expected;
  expected << '\\' << length << "-grams:";
  if (line != expected.str()) UTIL_THROW(FormatLoadException, "Was expecting n-gram header " << expected.str() << " but got " << line << " instead.  ");
}

template <class Voc> void Read1Grams(util::FilePiece &f, const size_t count, Voc &vocab, ProbBackoff *unigrams) {
  ReadNGramHeader(f, 1);
  for (size_t i = 0; i < count; ++i) {
    try {
      float prob = f.ReadFloat();
      if (f.get() != '\t') UTIL_THROW(FormatLoadException, "Expected tab after probability");
      ProbBackoff &value = unigrams[vocab.Insert(f.ReadDelimited())];
      value.prob = prob;
      switch (f.get()) {
        case '\t':
          value.SetBackoff(f.ReadFloat());
          if ((f.get() != '\n')) UTIL_THROW(FormatLoadException, "Expected newline after backoff");
          break;
        case '\n':
          value.ZeroBackoff();
          break;
        default:
          UTIL_THROW(FormatLoadException, "Expected tab or newline after unigram");
      }
     } catch(util::Exception &e) {
      e << " in the " << i << "th 1-gram at byte " << f.Offset();
      throw;
    }
  }
  if (f.ReadLine().size()) UTIL_THROW(FormatLoadException, "Expected blank line after unigrams at byte " << f.Offset());
}

template <class Voc, class Entry> void ReadNGram(util::FilePiece &f, const unsigned int n, const Voc &vocab, Entry &to) {
  try {
    to.weights.prob = f.ReadFloat();

    for (WordIndex *vocab_out = &to.words[n-1]; vocab_out >= to.words; --vocab_out) {
      *vocab_out = vocab.Index(f.ReadDelimited());
    }

    switch (f.get()) {
      case '\t':
        to.weights.SetBackoff(f.ReadFloat());
        if ((f.get() != '\n')) UTIL_THROW(FormatLoadException, "Expected newline after backoff");
        break;
      case '\n':
        to.weights.ZeroBackoff();
        break;
      default:
        UTIL_THROW(FormatLoadException, "Expected tab or newline after n-gram");
    }
  } catch(util::Exception &e) {
    e << " in the " <<  n << "-gram at byte " << f.Offset();
    throw;
  }
}

void WriteOrThrow(FILE *to, const void *data, size_t size) {
  if (size != std::fwrite(data, 1, size, to)) UTIL_THROW(util::ErrnoException, "Short write");
}

void ReadOrThrow(FILE *from, void *data, size_t size) {
  if (size != std::fread(data, 1, size, from)) UTIL_THROW(util::ErrnoException, "Short read");
}

template <class Entry> void DiskFlush(const Entry *begin, const Entry *end, const std::string &file_prefix, std::size_t batch, bool merged) {
  std::stringstream assembled;
  assembled << file_prefix << static_cast<unsigned int>(Entry::kOrder) << '_';
  if (merged) {
    assembled << "merged";
  } else {
    assembled << batch;
  }
  util::scoped_FILE out(fopen(assembled.str().c_str(), "w"));
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
}

template <class Entry> void ConvertToSorted(util::FilePiece &f, const lm::ngram::SortedVocabulary &vocab, const std::vector<size_t> &counts, util::scoped_memory &mem, const std::string &file_prefix) {
  ConvertToSorted<FullEntry<Entry::kOrder - 1> >(f, vocab, counts, mem, file_prefix);

  ReadNGramHeader(f, Entry::kOrder);
  const size_t count = counts[Entry::kOrder - 1];
  const size_t batch_size = std::min(count, mem.size() / sizeof(Entry));
  Entry *const begin = reinterpret_cast<Entry*>(mem.get());
  for (std::size_t batch = 0, done = 0; done < count; ++batch) {
    Entry *out = begin;
    Entry *out_end = out + std::min(count - done, batch_size);
    for (; out != out_end; ++out) {
      ReadNGram(f, Entry::kOrder, vocab, *out);
    }
    //__gnu_parallel::sort(begin, out_end);
    std::sort(begin, out_end);
    
    DiskFlush(begin, out_end, file_prefix, batch, count == batch_size);
    done += out_end - begin;
  }
}

template <> void ConvertToSorted<FullEntry<1> >(util::FilePiece &f, const lm::ngram::SortedVocabulary &vocab, const std::vector<size_t> &counts, util::scoped_memory &mem, const std::string &file_prefix) {}

void ARPAToSortedFiles(util::FilePiece &f, std::size_t buffer, const std::string &file_prefix, std::vector<size_t> &counts) {
  ReadARPACounts(f, counts);

  ngram::SortedVocabulary vocab;
  util::scoped_mapped_file vocab_file;
  std::string vocab_name = file_prefix + "vocab";
  util::MapZeroedWrite(vocab_name.c_str(), ngram::SortedVocabulary::Size(counts[0]), vocab_file);
  vocab.Init(vocab_file.mem.get(), ngram::SortedVocabulary::Size(counts[0]), counts[0]);

  {
    std::string unigram_name = file_prefix + "unigrams";
    util::scoped_mapped_file unigram_file;
    util::MapZeroedWrite(unigram_name.c_str(), counts[0] * sizeof(ProbBackoff), unigram_file);
    Read1Grams(f, counts[0], vocab, reinterpret_cast<ProbBackoff*>(unigram_file.mem.get()));
    vocab.FinishedLoading(reinterpret_cast<ProbBackoff*>(unigram_file.mem.get()));
  }

  util::scoped_memory mem;
  mem.reset(new char[buffer], buffer, util::scoped_memory::ARRAY_ALLOCATED);
  ConvertToSorted<lm::ProbEntry<5> >(f, vocab, counts, mem, file_prefix);
}

class SortedFileReader {
  public:
    SortedFileReader() {}

    void Init(const std::string &name, unsigned char order) {
      file_.reset(fopen(name.c_str(), "r"));
      if (!file_.get()) UTIL_THROW(util::ErrnoException, "Opening " << name << " for read");
      header_.resize(order - 1);
      ReadHeader();
    }

    // Preceding words.
    const WordIndex *Header() const {
      return &*header_.begin();
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

  private:
    void ReadHeader() {
      ReadOrThrow(file_.get(), &*header_.begin(), sizeof(WordIndex) * header_.size());
    }

    util::scoped_FILE file_;

    std::vector<WordIndex> header_;
};


struct MiddleValue {
  ProbBackoff weights;
  uint64_t next;
  uint64_t Next() const { return next; }
};

struct EndValue {
  Prob weights;
  uint64_t Next() const { return 0; }
};

template <class Value> class SimpleTrie {
  private:
    struct Entry {
      WordIndex key;
      WordIndex GetKey() const { return key; }
      Value value;
    };

  public:
    SimpleTrie() {}

    void Init(void *mem, std::size_t entries) {
      begin_ = reinterpret_cast<const Entry*>(mem);
      end_ = reinterpret_cast<Entry*>(mem);
    }

    bool Find(uint64_t offset, std::size_t delta, WordIndex key, Value &out, uint64_t &delta_out) const {
      const Entry *found;
      if (!util::SortedUniformFind(begin_ + offset, begin_ + offset + delta, key, found)) return false;
      out = found->value;
      delta_out = (found+1)->value.Next() - found->value.Next();
      return true;
    }

    static std::size_t Size(std::size_t entries) {
      return entries * sizeof(Entry);
    }

    std::size_t NextOffset() const {
      return end_ - begin_;
    }

    class Inserter {
      public:
        Inserter(SimpleTrie<Value> &in, std::size_t size) : begin_(in.begin_), end_(in.end_) {}

        void Add(WordIndex key, const Value &value) {
          end_->key = key;
          end_->value = value;
          ++end_;
        }

      private:
        const Entry *begin_;
        Entry *&end_;
    };

  private:
    friend class Inserter;
    const Entry *begin_;
    Entry *end_;
};

struct RecursiveInsertParams {
  WordIndex *words;
  SortedFileReader *files;
  unsigned char max_order;
  SimpleTrie<MiddleValue> *middle;
  SimpleTrie<EndValue> *longest;
};

uint64_t RecursiveInsert(RecursiveInsertParams &params, unsigned char order) {
  SortedFileReader &file = params.files[order - 2];
  if (std::memcmp(params.words, file.Header(), sizeof(WordIndex) * (order - 1)))
    return std::numeric_limits<uint64_t>::max();
  WordIndex count;
  file.ReadCount(count);
  WordIndex key;
  uint64_t ret;
  if (order == params.max_order) {
    ret = params.longest->NextOffset();
    SimpleTrie<EndValue>::Inserter inserter(*params.longest, count);
    EndValue value;
    for (WordIndex i = 0; i < count; ++i) {
      file.ReadWord(key);
      file.ReadWeights(value.weights);
      inserter.Add(key, value);
    }
    return ret;
  } else {
    ret = params.middle[order - 2].NextOffset();
    SimpleTrie<MiddleValue>::Inserter inserter(params.middle[order - 2], count);
    MiddleValue value;
    for (WordIndex i = 0; i < count; ++i) {
      file.ReadWord(params.words[order - 1]);
      value.next = RecursiveInsert(params, order + 1);
      file.ReadWeights(value.weights);
      inserter.Add(params.words[order - 1], value);
    }
  }
  return ret;
}

void BuildTrie(const std::string &file_prefix, const std::vector<std::size_t> &counts) {
  std::vector<MiddleValue> unigrams(counts[0]);

  std::vector<std::vector<char> > middle_mem(counts.size() - 2);
  std::vector<SimpleTrie<MiddleValue> > middle;
  for (size_t i = 0; i < middle.size(); ++i) {
    middle_mem[i].resize(SimpleTrie<MiddleValue>::Size(counts[i + 1]));
    middle[i].Init(&*middle_mem.begin(), counts[i+1]);
  }
  std::vector<char> longest_mem(SimpleTrie<Prob>::Size(counts.back()));
  SimpleTrie<EndValue> longest;
  longest.Init(&*longest_mem.begin(), counts.back());

  // Load unigrams.  Leave the next pointers uninitialized.   
  {
    std::string name(file_prefix + "unigrams");
    util::scoped_FILE file(fopen(name.c_str(), "r"));
    if (!file.get()) UTIL_THROW(util::ErrnoException, "Opening " << name << " failed");
    for (WordIndex i = 0; i < counts[0]; ++i) {
      ReadOrThrow(file.get(), &unigrams[i].weights, sizeof(ProbBackoff));
    }
  }

  // inputs[0] is bigrams.
  SortedFileReader inputs[counts.size() - 1];
  for (unsigned char i = 2; i <= counts.size(); ++i) {
    std::stringstream assembled;
    assembled << file_prefix << i << "_merged";
    inputs[i-2].Init(assembled.str(), i);
  }

  // words[0] is unigrams.  
  WordIndex words[counts.size()];
  RecursiveInsertParams params;
  params.words = words;
  params.files = inputs;
  params.max_order = static_cast<unsigned char>(counts.size());
  params.middle = &*middle.begin();
  params.longest = &longest;

  for (words[0] = 0; words[0] < counts[0]; ++words[0]) {
    unigrams[words[0]].next = RecursiveInsert(params, 2);
  }
}

} // namespace lm

int main() {
  util::FilePiece f("stdin", 0);
  std::vector<std::size_t> counts;
  lm::ARPAToSortedFiles(f, 1073741824ULL, "sort/", counts);
  // TODO: merge.

}

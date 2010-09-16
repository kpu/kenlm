#include "lm/exception.hh"
#include "lm/ngram.hh"
#include "lm/word_index.hh"
#include "util/file_piece.hh"
#include "util/scoped.hh"

#include <algorithm>
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
  if (size != fwrite(data, 1, size, to)) UTIL_THROW(util::ErrnoException, "Short write");
}

template <class Entry> void DiskFlush(const Entry *begin, const Entry *end, const std::string &file_prefix, std::size_t batch) {
  std::stringstream assembled;
  assembled << file_prefix << static_cast<unsigned int>(Entry::kOrder) << '_' << batch;
  util::scoped_FILE out(fopen(assembled.str().c_str(), "w"));
  for (const Entry *group_begin = begin; group_begin != end;) {
    const Entry *group_end = group_begin;
    for (++group_end; (group_end != end) && !memcmp(group_begin->words, group_end->words, sizeof(WordIndex) * (Entry::kOrder - 1)); ++group_end) {}
    WriteOrThrow(out.get(), group_begin->words, sizeof(WordIndex) * (Entry::kOrder - 1));
    uint64_t group_size = group_end - group_begin;
    WriteOrThrow(out.get(), &group_size, sizeof(uint64_t));
    for (const Entry *i = group_begin; i != group_end; ++i) {
      WriteOrThrow(out.get(), &i->words[Entry::kOrder - 1], sizeof(WordIndex));
      WriteOrThrow(out.get(), &i->weights, sizeof(typename Entry::Weights));
    }
    group_begin = group_end;
  }
}

template <class Entry> void RecursiveSort(util::FilePiece &f, const lm::ngram::SortedVocabulary &vocab, const std::vector<size_t> &counts, util::scoped_memory &mem, const std::string &file_prefix) {
  RecursiveSort<FullEntry<Entry::kOrder - 1> >(f, vocab, counts, mem, file_prefix);

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
    
    DiskFlush(begin, out_end, file_prefix, batch);
    done += out_end - begin;
  }
}

template <> void RecursiveSort<FullEntry<1> >(util::FilePiece &f, const lm::ngram::SortedVocabulary &vocab, const std::vector<size_t> &counts, util::scoped_memory &mem, const std::string &file_prefix) {}

void OpenForZeroedMMAP(const char *name, std::size_t size, util::scoped_fd &fd, util::scoped_mmap &mem) {
  fd.reset(open(name, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));
  if (-1 == fd.get()) UTIL_THROW(util::ErrnoException, "Failed to open " << name << " file for writing.");
  if (-1 == ftruncate(fd.get(), size)) UTIL_THROW(util::ErrnoException, "ftruncate on " << name << " to " << size << " failed.");
  mem.reset(mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, fd.get(), 0), size);
}

void ARPAToSortedFiles(util::FilePiece &f, std::size_t buffer, const std::string &file_prefix) {
  std::vector<std::size_t> counts;
  ReadARPACounts(f, counts);

  ngram::SortedVocabulary vocab;
  util::scoped_fd vocab_file; util::scoped_mmap vocab_mem;
  OpenForZeroedMMAP((file_prefix + "vocab").c_str(), ngram::SortedVocabulary::Size(counts[0]), vocab_file, vocab_mem);
  vocab.Init(vocab_mem.get(), ngram::SortedVocabulary::Size(counts[0]), counts[0]);

  {
    util::scoped_fd unigram_file; util::scoped_mmap unigram_mem;
    OpenForZeroedMMAP((file_prefix + "unigrams").c_str(), counts[0] * sizeof(ProbBackoff), unigram_file, unigram_mem);
    Read1Grams(f, counts[0], vocab, reinterpret_cast<ProbBackoff*>(unigram_mem.get()));
    vocab.FinishedLoading(reinterpret_cast<ProbBackoff*>(unigram_mem.get()));
  }

  util::scoped_memory mem;
  mem.reset(new char[buffer], buffer, util::scoped_memory::ARRAY_ALLOCATED);
  RecursiveSort<lm::ProbEntry<5> >(f, vocab, counts, mem, file_prefix);
}

} // namespace lm

int main() {
  util::FilePiece f("stdin", 0);
  lm::ARPAToSortedFiles(f, 1073741824ULL, "sort/");
  // TODO: merge.
}

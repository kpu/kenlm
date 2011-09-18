/* This is where the trie is built.  It's on-disk.  */
#include "lm/search_trie.hh"

#include "lm/binary_format.hh"
#include "lm/bhiksha.hh"
#include "lm/blank.hh"
#include "lm/lm_exception.hh"
#include "lm/max_order.hh"
#include "lm/quantize.hh"
#include "lm/trie.hh"
#include "lm/trie_sort.hh"
#include "lm/vocab.hh"
#include "lm/weights.hh"
#include "lm/word_index.hh"
#include "util/ersatz_progress.hh"
#include "util/file_piece.hh"
#include "util/have.hh"
#include "util/proxy_iterator.hh"
#include "util/sized_iterator.hh"
#include "util/scoped.hh"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <queue>
#include <limits>
#include <numeric>
#include <vector>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

namespace lm {
namespace ngram {
namespace trie {
namespace {

// Phase to count n-grams, including blanks inserted because they were pruned but have extensions
class JustCount {
  public:
    JustCount(uint64_t *counts, unsigned char order)
      : counts_(counts), longest_counts_(counts + order - 1) {}

    float UnigramProb(WordIndex /*index*/) { return 0.0; }

    void Unigram(WordIndex /*index*/) {
      ++counts_[0];
    }

    void MiddleBlank(const unsigned char order, WordIndex /* idx */, unsigned char /*lower*/, float /* prob_base */) {
      ++counts_[order - 1];
    }

    void Middle(const unsigned char order, const void * /*data*/) {
      ++counts_[order - 1];
    }

    void Longest(const void * /*data*/) {
      ++*longest_counts_;
    }

    // Unigrams wrote one past.  
    void Cleanup() {
      --counts_[0];
    }

  private:
    uint64_t *const counts_, *const longest_counts_;
};

// Phase to actually write n-grams to the trie.  
template <class Quant, class Bhiksha> class WriteEntries {
  public:
    WriteEntries(RecordReader *contexts, UnigramValue *unigrams, BitPackedMiddle<typename Quant::Middle, Bhiksha> *middle, BitPackedLongest<typename Quant::Longest> &longest, unsigned char order) : 
      contexts_(contexts),
      unigrams_(unigrams),
      middle_(middle),
      longest_(longest), 
      bigram_pack_((order == 2) ? static_cast<BitPacked&>(longest_) : static_cast<BitPacked&>(*middle_)), order_(order) {}

    float UnigramProb(WordIndex index) const { return unigrams_[index].weights.prob; }

    void Unigram(WordIndex word) {
      unigrams_[word].next = bigram_pack_.InsertIndex();
    }

    void MiddleBlank(const unsigned char order, WordIndex word, unsigned char lower, float prob_base) {
      middle_[order - 1].Insert(word, kBlankProb, kBlankBackoff);
    }

    void Middle(const unsigned char order, const void *data) {
      RecordReader &context = contexts_[order - 1];
      const WordIndex *words = reinterpret_cast<const WordIndex*>(data);
      ProbBackoff weights = *reinterpret_cast<const ProbBackoff*>(words + order);
      if (context && !memcmp(data, context.Data(), sizeof(WordIndex) * order)) {
        SetExtension(weights.backoff);
        ++context;
      }
      middle_[order - 1].Insert(words[order - 1], weights);
    }

    void Longest(const void *data) {
      const WordIndex *words = reinterpret_cast<const WordIndex*>(data);
      longest_.Insert(words[order_ - 1], reinterpret_cast<const Prob*>(words + order)->prob);
    }

    void Cleanup() {}

  private:
    ContextReader *contexts_;
    UnigramValue *const unigrams_;
    BitPackedMiddle<typename Quant::Middle, Bhiksha> *const middle_;
    BitPackedLongest<typename Quant::Longest> &longest_;
    BitPacked &bigram_pack_;
    const unsigned char order_;
};

struct Gram {
  Gram(const WordIndex *in_begin, unsigned char order) : begin(in_begin), end(in_begin + order) {}

  const WordIndex *begin, *end;

  // For queue, this is the direction we want.  
  bool operator<(const Gram &other) const {
    return std::lexicographic_compare(other.begin, other.end, begin, end);
  }
};

template <class Doing> class BlankManager {
  private:
    static float kBadProb = std::numeric_limits<float>::infinity();

  public:
    BlankManager(unsigned char total_order, Doing &doing) : total_order_(total_order), been_length_(0), doing_(doing) {
      for (float *i = basis_; i != basis_ + kMaxOrder - 1; ++i) *i = kBadProb;
    }

    void Visit(const WordIndex *to, unsigned char length, float prob) {
      basis_[length - 1] = prob;
      unsigned char overlap = std::min(length - 1, been_length_);
      const WordIndex *cur, *pre;
      for (cur = to, pre = been_; cur != to + overlap; ++cur, ++pre) {
        if (*pre != *cur) break;
      }
      if (cur == to + length - 1) {
        *pre = *cur;
        been_length_ = length;
        return;
      }
      // There are blanks to insert starting with order blank.  
      unsigned char blank = cur - to + 1;
      const float *lower_basis;
      for (lower_basis = basis_ + blank - 1; (lower_basis > basis_) && *lower_basis = kBadProb; --lower_basis) {}
      unsigned char based_on = lower_basis - basis_ + 1;
      for (; cur != to + length - 1; ++blank, ++cur, ++pre) {
        doing_.MiddleBlank(blank, *cur, based_on, *lower_basis);
        *pre = *cur;
        // Mark that the probability is a blank so it shouldn't be used as the basis for a later n-gram.  
        basis_[blank - 1] = kBadProb;
      }
      been_length_ = length;
    }

  private:
    const unsigned char total_order_;

    WordIndex been_[kMaxOrder];
    unsigned char been_length_;

    float basis_[kMaxOrder];
    
    Doing &doing_;
};

template <class Doing> void RecursiveInsert(const unsigned char total_order, const WordIndex unigram_count, RecordReader *input, std::ostream *progress_out, const char *message, Doing &out) {
  util::ErsatzProgress progress(progress_out, message, unigram_count + 1);
  unsigned int unigram = 0;
  std::priority_queue<Gram> grams;
  grams.push(Gram(&unigram, 1));
  for (unsigned char i = 2; i <= total_order; ++i) {
    grams.push(Gram(inputs[i-2].Data(), i));
  }

  BlankManager<Doing> blank(total_order, doing);

  while (true) {
    Gram top = gram.top();
    gram.pop();
    unsigned char order = top.end - top.begin;
    if (order == 1) {
      blank.Visit(&unigram, 1, doing_.UnigramProb(unigram));
      doing_.Unigram(unigram);
      if (++unigram == unigram_count + 1) break;
      gram.push(top);
    } else {
      if (order == total_order) {
        blank.Visit(top.begin, order, reinterpret_cast<const Prob*>(top.end)->prob);
        doing_.Longest(top.begin);
      } else {
        blank.Visit(top.begin, order, reinterpret_cast<const ProbBackoff*>(top.end)->prob);
        doing_.Middle(order, top.begin);
      }
      RecordReader &reader = inputs[order - 2];
      if (++reader) gram.push(top);
    }
  }
  assert(gram.empty());
}


void SanityCheckCounts(const std::vector<uint64_t> &initial, const std::vector<uint64_t> &fixed) {
  if (fixed[0] != initial[0]) UTIL_THROW(util::Exception, "Unigram count should be constant but initial is " << initial[0] << " and recounted is " << fixed[0]);
  if (fixed.back() != initial.back()) UTIL_THROW(util::Exception, "Longest count should be constant but it changed from " << initial.back() << " to " << fixed.back());
  for (unsigned char i = 0; i < initial.size(); ++i) {
    if (fixed[i] < initial[i]) UTIL_THROW(util::Exception, "Counts came out lower than expected.  This shouldn't happen");
  }
}

bool IsDirectory(const char *path) {
  struct stat info;
  if (0 != stat(path, &info)) return false;
  return S_ISDIR(info.st_mode);
}

template <class Quant> void TrainQuantizer(uint8_t order, uint64_t count, SortedFileReader &reader, util::ErsatzProgress &progress, Quant &quant) {
  ProbBackoff weights;
  std::vector<float> probs, backoffs;
  probs.reserve(count);
  backoffs.reserve(count);
  for (reader.Rewind(); !reader.Ended(); reader.NextHeader()) {
    uint64_t entries = reader.ReadCount();
    for (uint64_t c = 0; c < entries; ++c) {
      reader.ReadWord();
      reader.ReadWeights(weights);
      // kBlankProb isn't added yet.  
      probs.push_back(weights.prob);
      if (weights.backoff != 0.0) backoffs.push_back(weights.backoff);
      ++progress;
    }
  }
  quant.Train(order, probs, backoffs);
}

template <class Quant> void TrainProbQuantizer(uint8_t order, uint64_t count, SortedFileReader &reader, util::ErsatzProgress &progress, Quant &quant) {
  Prob weights;
  std::vector<float> probs, backoffs;
  probs.reserve(count);
  for (reader.Rewind(); !reader.Ended(); reader.NextHeader()) {
    uint64_t entries = reader.ReadCount();
    for (uint64_t c = 0; c < entries; ++c) {
      reader.ReadWord();
      reader.ReadWeights(weights);
      // kBlankProb isn't added yet.  
      probs.push_back(weights.prob);
      ++progress;
    }
  }
  quant.TrainProb(order, probs);
}

} // namespace

template <class Quant, class Bhiksha> void BuildTrie(const std::string &file_prefix, std::vector<uint64_t> &counts, const Config &config, TrieSearch<Quant, Bhiksha> &out, Quant &quant, const SortedVocabulary &vocab, Backing &backing) {
  std::vector<SortedFileReader> inputs(counts.size() - 1);
  std::vector<ContextReader> contexts(counts.size() - 1);

  for (unsigned char i = 2; i <= counts.size(); ++i) {
    std::stringstream assembled;
    assembled << file_prefix << static_cast<unsigned int>(i) << "_merged";
    inputs[i-2].Init(assembled.str(), i);
    RemoveOrThrow(assembled.str().c_str());
    assembled << kContextSuffix;
    contexts[i-2].Reset(assembled.str().c_str(), i-1);
    RemoveOrThrow(assembled.str().c_str());
  }

  std::vector<uint64_t> fixed_counts(counts.size());
  {
    RecursiveInsert<JustCount> counter(&*inputs.begin(), &*contexts.begin(), NULL, out.middle_begin_, out.longest, &*fixed_counts.begin(), counts.size());
    counter.Apply(config.messages, "Counting n-grams that should not have been pruned", counts[0]);
  }
  for (std::vector<SortedFileReader>::const_iterator i = inputs.begin(); i != inputs.end(); ++i) {
    if (!i->Ended()) UTIL_THROW(FormatLoadException, "There's a bug in the trie implementation: the " << (i - inputs.begin() + 2) << "-gram table did not complete reading");
  }
  SanityCheckCounts(counts, fixed_counts);
  counts = fixed_counts;

  out.SetupMemory(GrowForSearch(config, vocab.UnkCountChangePadding(), TrieSearch<Quant, Bhiksha>::Size(fixed_counts, config), backing), fixed_counts, config);

  if (Quant::kTrain) {
    util::ErsatzProgress progress(config.messages, "Quantizing", std::accumulate(counts.begin() + 1, counts.end(), 0));
    for (unsigned char i = 2; i < counts.size(); ++i) {
      TrainQuantizer(i, counts[i-1], inputs[i-2], progress, quant);
    }
    TrainProbQuantizer(counts.size(), counts.back(), inputs[counts.size() - 2], progress, quant);
    quant.FinishedLoading(config);
  }

  for (unsigned char i = 2; i <= counts.size(); ++i) {
    inputs[i-2].Rewind();
  }
  UnigramValue *unigrams = out.unigram.Raw();
  // Fill entries except unigram probabilities.  
  {
    RecursiveInsert<WriteEntries<Quant, Bhiksha> > inserter(&*inputs.begin(), &*contexts.begin(), unigrams, out.middle_begin_, out.longest, &*fixed_counts.begin(), counts.size());
    inserter.Apply(config.messages, "Building trie", fixed_counts[0]);
  }

  // Fill unigram probabilities.  
  try {
    std::string name(file_prefix + "unigrams");
    util::scoped_FILE file(OpenOrThrow(name.c_str(), "r"));
    for (WordIndex i = 0; i < counts[0]; ++i) {
      ReadOrThrow(file.get(), &unigrams[i].weights, sizeof(ProbBackoff));
      if (contexts[0] && **contexts[0] == i) {
        SetExtension(unigrams[i].weights.backoff);
        ++contexts[0];
      }
    }
    RemoveOrThrow(name.c_str());
  } catch (util::Exception &e) {
    e << " while re-reading unigram probabilities";
    throw;
  }

  // Do not disable this error message or else too little state will be returned.  Both WriteEntries::Middle and returning state based on found n-grams will need to be fixed to handle this situation.   
  for (unsigned char order = 2; order <= counts.size(); ++order) {
    const ContextReader &context = contexts[order - 2];
    if (context) {
      FormatLoadException e;
      e << "An " << static_cast<unsigned int>(order) << "-gram has the context (i.e. all but the last word):";
      for (const WordIndex *i = *context; i != *context + order - 1; ++i) {
        e << ' ' << *i;
      }
      e << " so this context must appear in the model as a " << static_cast<unsigned int>(order - 1) << "-gram but it does not";
      throw e;
    }
  }

  /* Set ending offsets so the last entry will be sized properly */
  // Last entry for unigrams was already set.  
  if (out.middle_begin_ != out.middle_end_) {
    for (typename TrieSearch<Quant, Bhiksha>::Middle *i = out.middle_begin_; i != out.middle_end_ - 1; ++i) {
      i->FinishedLoading((i+1)->InsertIndex(), config);
    }
    (out.middle_end_ - 1)->FinishedLoading(out.longest.InsertIndex(), config);
  }  
}

template <class Quant, class Bhiksha> uint8_t *TrieSearch<Quant, Bhiksha>::SetupMemory(uint8_t *start, const std::vector<uint64_t> &counts, const Config &config) {
  quant_.SetupMemory(start, config);
  start += Quant::Size(counts.size(), config);
  unigram.Init(start);
  start += Unigram::Size(counts[0]);
  FreeMiddles();
  middle_begin_ = static_cast<Middle*>(malloc(sizeof(Middle) * (counts.size() - 2)));
  middle_end_ = middle_begin_ + (counts.size() - 2);
  std::vector<uint8_t*> middle_starts(counts.size() - 2);
  for (unsigned char i = 2; i < counts.size(); ++i) {
    middle_starts[i-2] = start;
    start += Middle::Size(Quant::MiddleBits(config), counts[i-1], counts[0], counts[i], config);
  }
  // Crazy backwards thing so we initialize using pointers to ones that have already been initialized
  for (unsigned char i = counts.size() - 1; i >= 2; --i) {
    new (middle_begin_ + i - 2) Middle(
        middle_starts[i-2],
        quant_.Mid(i),
        counts[i-1],
        counts[0],
        counts[i],
        (i == counts.size() - 1) ? static_cast<const BitPacked&>(longest) : static_cast<const BitPacked &>(middle_begin_[i-1]),
        config);
  }
  longest.Init(start, quant_.Long(counts.size()), counts[0]);
  return start + Longest::Size(Quant::LongestBits(config), counts.back(), counts[0]);
}

template <class Quant, class Bhiksha> void TrieSearch<Quant, Bhiksha>::LoadedBinary() {
  unigram.LoadedBinary();
  for (Middle *i = middle_begin_; i != middle_end_; ++i) {
    i->LoadedBinary();
  }
  longest.LoadedBinary();
}

template <class Quant, class Bhiksha> void TrieSearch<Quant, Bhiksha>::InitializeFromARPA(const char *file, util::FilePiece &f, std::vector<uint64_t> &counts, const Config &config, SortedVocabulary &vocab, Backing &backing) {
  std::string temporary_directory;
  if (config.temporary_directory_prefix) {
    temporary_directory = config.temporary_directory_prefix;
    if (!temporary_directory.empty() && temporary_directory[temporary_directory.size() - 1] != '/' && IsDirectory(temporary_directory.c_str()))
      temporary_directory += '/';
  } else if (config.write_mmap) {
    temporary_directory = config.write_mmap;
  } else {
    temporary_directory = file;
  }
  // Null on end is kludge to ensure null termination.
  temporary_directory += "_trie_tmp_XXXXXX";
  temporary_directory += '\0';
  if (!mkdtemp(&temporary_directory[0])) {
    UTIL_THROW(util::ErrnoException, "Failed to make a temporary directory based on the name " << temporary_directory.c_str());
  }
  // Chop off null kludge.  
  temporary_directory.resize(strlen(temporary_directory.c_str()));
  // Add directory delimiter.  Assumes a real operating system.  
  temporary_directory += '/';
  // At least 1MB sorting memory.  
  ARPAToSortedFiles(config, f, counts, std::max<size_t>(config.building_memory, 1048576), temporary_directory.c_str(), vocab);

  BuildTrie(temporary_directory, counts, config, *this, quant_, vocab, backing);
  if (rmdir(temporary_directory.c_str()) && config.messages) {
    *config.messages << "Failed to delete " << temporary_directory << std::endl;
  }
}

template class TrieSearch<DontQuantize, DontBhiksha>;
template class TrieSearch<DontQuantize, ArrayBhiksha>;
template class TrieSearch<SeparatelyQuantize, DontBhiksha>;
template class TrieSearch<SeparatelyQuantize, ArrayBhiksha>;

} // namespace trie
} // namespace ngram
} // namespace lm

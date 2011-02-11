#include "lm/search_hashed.hh"

#include "lm/blank.hh"
#include "lm/lm_exception.hh"
#include "lm/read_arpa.hh"
#include "lm/vocab.hh"

#include "util/file_piece.hh"

#include <string>

namespace lm {
namespace ngram {

namespace {

/* These are passed to ReadNGrams so that n-grams with zero backoff that appear as context will still be used in state. */
template <class Middle> class ActivateLowerMiddle {
  public:
    explicit ActivateLowerMiddle(Middle &middle) : modify_(middle) {}

    void operator()(const WordIndex *vocab_ids, const unsigned int n) {
      uint64_t hash = static_cast<WordIndex>(vocab_ids[1]);
      for (const WordIndex *i = vocab_ids + 2; i < vocab_ids + n; ++i) {
        hash = detail::CombineWordHash(hash, *i);
      }
      typename Middle::MutableIterator i;
      // TODO: somehow get text of n-gram for this error message.
      if (!modify_.UnsafeMutableFind(hash, i))
        UTIL_THROW(FormatLoadException, "The context of every " << n << "-gram should appear as a " << (n-1) << "-gram");
      SetExtension(i->MutableValue().backoff);
    }

  private:
    Middle &modify_;
};

class ActivateUnigram {
  public:
    explicit ActivateUnigram(ProbBackoff *unigram) : modify_(unigram) {}

    void operator()(const WordIndex *vocab_ids, const unsigned int /*n*/) {
      // assert(n == 2);
      SetExtension(modify_[vocab_ids[1]].backoff);
    }

  private:
    ProbBackoff *modify_;
};

template <class Voc, class Store, class Middle, class Activate> void ReadNGrams(util::FilePiece &f, const unsigned int n, const size_t count, const Voc &vocab, std::vector<Middle> &middle, Activate activate, Store &store) {
  
  ReadNGramHeader(f, n);
  ProbBackoff blank;
  blank.prob = kBlankProb;
  blank.backoff = kBlankBackoff;

  // vocab ids of words in reverse order
  WordIndex vocab_ids[n];
  uint64_t keys[n - 1];
  typename Store::Packing::Value value;
  typename Middle::ConstIterator found;
  for (size_t i = 0; i < count; ++i) {
    ReadNGram(f, n, vocab, vocab_ids, value);
    keys[0] = detail::CombineWordHash(static_cast<uint64_t>(*vocab_ids), vocab_ids[1]);
    for (unsigned int h = 1; h < n - 1; ++h) {
      keys[h] = detail::CombineWordHash(keys[h-1], vocab_ids[h+1]);
    }
    store.Insert(Store::Packing::Make(keys[n-2], value));
    // Go back and insert blanks.  
    for (int lower = n - 3; lower >= 0; --lower) {
      if (middle[lower].Find(keys[lower], found)) break;
      middle[lower].Insert(Middle::Packing::Make(keys[lower], blank));
    }
    activate(vocab_ids, n);
  }

  store.FinishedInserting();
}

} // namespace
namespace detail {

template <class MiddleT, class LongestT> template <class Voc> void TemplateHashedSearch<MiddleT, LongestT>::InitializeFromARPA(const char * /*file*/, util::FilePiece &f, const std::vector<uint64_t> &counts, const Config &config, Voc &vocab, Backing &backing) {
  // TODO: fix sorted.
  SetupMemory(GrowForSearch(config, HASH_PROBING, counts, Size(counts, config), backing), counts, config);

  Read1Grams(f, counts[0], vocab, unigram.Raw());  

  try {
    if (counts.size() > 2) {
      ReadNGrams(f, 2, counts[1], vocab, middle, ActivateUnigram(unigram.Raw()), middle[0]);
    }
    for (unsigned int n = 3; n < counts.size(); ++n) {
      ReadNGrams(f, n, counts[n-1], vocab, middle, ActivateLowerMiddle<Middle>(middle[n-3]), middle[n-2]);
    }
    if (counts.size() > 2) {
      ReadNGrams(f, counts.size(), counts[counts.size() - 1], vocab, middle, ActivateLowerMiddle<Middle>(middle.back()), longest);
    } else {
      ReadNGrams(f, counts.size(), counts[counts.size() - 1], vocab, middle, ActivateUnigram(unigram.Raw()), longest);
    }
  } catch (util::ProbingSizeException &e) {
    UTIL_THROW(util::ProbingSizeException, "Avoid pruning n-grams like \"bar baz quux\" when \"foo bar baz quux\" is still in the model.  KenLM will work when this pruning happens, but the probing model assumes these events are rare enough that using blank space in the probing hash table will cover all of them.  Increase probing_multiplier (-p to build_binary) to add more blank spaces.\n");
  }
  ReadEnd(f);
}

template void TemplateHashedSearch<ProbingHashedSearch::Middle, ProbingHashedSearch::Longest>::InitializeFromARPA(const char *, util::FilePiece &f, const std::vector<uint64_t> &counts, const Config &, ProbingVocabulary &vocab, Backing &backing);
template void TemplateHashedSearch<SortedHashedSearch::Middle, SortedHashedSearch::Longest>::InitializeFromARPA(const char *, util::FilePiece &f, const std::vector<uint64_t> &counts, const Config &, SortedVocabulary &vocab, Backing &backing);

SortedHashedSearch::SortedHashedSearch() {
  UTIL_THROW(util::Exception, "Sorted is broken at the moment, sorry");
}

} // namespace detail
} // namespace ngram
} // namespace lm

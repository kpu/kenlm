#include "lm/search_hashed.hh"

#include "lm/blank.hh"
#include "lm/lm_exception.hh"
#include "lm/read_arpa.hh"
#include "lm/vocab.hh"

#include "util/bit_packing.hh"
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

template <class Voc, class Store, class Middle, class Activate> void ReadNGrams(util::FilePiece &f, const unsigned int n, const size_t count, const Voc &vocab, ProbBackoff *unigrams, std::vector<Middle> &middle, Activate activate, Store &store, PositiveProbWarn &warn) {
  ReadNGramHeader(f, n);
  ProbBackoff blank;
  blank.prob = kBlankProb;
  // Unset sign bit of kBlankProb to indicate it extends left.
  util::UnsetSign(blank.prob);
  blank.backoff = kBlankBackoff;

  // vocab ids of words in reverse order
  WordIndex vocab_ids[n];
  uint64_t keys[n - 1];
  typename Store::Packing::Value value;
  typename Middle::MutableIterator found;
  for (size_t i = 0; i < count; ++i) {
    ReadNGram(f, n, vocab, vocab_ids, value, warn);

    keys[0] = detail::CombineWordHash(static_cast<uint64_t>(*vocab_ids), vocab_ids[1]);
    for (unsigned int h = 1; h < n - 1; ++h) {
      keys[h] = detail::CombineWordHash(keys[h-1], vocab_ids[h+1]);
    }
    // Initially the sign bit is on, indicating it does not extend left.  Most already have this but there might +0.0.  
    util::SetSign(value.prob);
    store.Insert(Store::Packing::Make(keys[n-2], value));
    // Go back and insert blanks and set sign to indicate that entries extend left.  
    for (int lower = n - 3; ; --lower) {
      if (lower == -1) {
        util::UnsetSign(unigrams[vocab_ids[0]].prob);
        break;
      }
      if (middle[lower].UnsafeMutableFind(keys[lower], found)) {
        // Turn off sign bit to indicate that it extends left.  
        util::UnsetSign(found->MutableValue().prob);
        // We don't need to recurse further down because this entry already set the bits for lower entries.  
        break;
      }
      middle[lower].Insert(Middle::Packing::Make(keys[lower], blank));
    }
    activate(vocab_ids, n);
  }

  store.FinishedInserting();
}

} // namespace
namespace detail {
 
template <class MiddleT, class LongestT> uint8_t *TemplateHashedSearch<MiddleT, LongestT>::SetupMemory(uint8_t *start, const std::vector<uint64_t> &counts, const Config &config) {
  std::size_t allocated = Unigram::Size(counts[0]);
  unigram = Unigram(start, allocated);
  start += allocated;
  for (unsigned int n = 2; n < counts.size(); ++n) {
    allocated = Middle::Size(counts[n - 1], config.probing_multiplier);
    middle_.push_back(Middle(start, allocated));
    start += allocated;
  }
  allocated = Longest::Size(counts.back(), config.probing_multiplier);
  longest = Longest(start, allocated);
  start += allocated;
  return start;
}

template <class MiddleT, class LongestT> template <class Voc> void TemplateHashedSearch<MiddleT, LongestT>::InitializeFromARPA(const char * /*file*/, util::FilePiece &f, const std::vector<uint64_t> &counts, const Config &config, Voc &vocab, Backing &backing) {
  // TODO: fix sorted.
  SetupMemory(GrowForSearch(config, 0, Size(counts, config), backing), counts, config);

  PositiveProbWarn warn(config.positive_log_probability);

  Read1Grams(f, counts[0], vocab, unigram.Raw(), warn);
  CheckSpecials(config, vocab);

  try {
    if (counts.size() > 2) {
      ReadNGrams(f, 2, counts[1], vocab, unigram.Raw(), middle_, ActivateUnigram(unigram.Raw()), middle_[0], warn);
    }
    for (unsigned int n = 3; n < counts.size(); ++n) {
      ReadNGrams(f, n, counts[n-1], vocab, unigram.Raw(), middle_, ActivateLowerMiddle<Middle>(middle_[n-3]), middle_[n-2], warn);
    }
    if (counts.size() > 2) {
      ReadNGrams(f, counts.size(), counts[counts.size() - 1], vocab, unigram.Raw(), middle_, ActivateLowerMiddle<Middle>(middle_.back()), longest, warn);
    } else {
      ReadNGrams(f, counts.size(), counts[counts.size() - 1], vocab, unigram.Raw(), middle_, ActivateUnigram(unigram.Raw()), longest, warn);
    }
  } catch (util::ProbingSizeException &e) {
    UTIL_THROW(util::ProbingSizeException, "Avoid pruning n-grams like \"bar baz quux\" when \"foo bar baz quux\" is still in the model.  KenLM will work when this pruning happens, but the probing model assumes these events are rare enough that using blank space in the probing hash table will cover all of them.  Increase probing_multiplier (-p to build_binary) to add more blank spaces.\n");
  }
  ReadEnd(f);
}

template <class MiddleT, class LongestT> void TemplateHashedSearch<MiddleT, LongestT>::LoadedBinary() {
  unigram.LoadedBinary();
  for (typename std::vector<Middle>::iterator i = middle_.begin(); i != middle_.end(); ++i) {
    i->LoadedBinary();
  }
  longest.LoadedBinary();
}

template class TemplateHashedSearch<ProbingHashedSearch::Middle, ProbingHashedSearch::Longest>;

template void TemplateHashedSearch<ProbingHashedSearch::Middle, ProbingHashedSearch::Longest>::InitializeFromARPA(const char *, util::FilePiece &f, const std::vector<uint64_t> &counts, const Config &, ProbingVocabulary &vocab, Backing &backing);

} // namespace detail
} // namespace ngram
} // namespace lm

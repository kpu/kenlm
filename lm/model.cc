#include "lm/model.hh"

#include "lm/blank.hh"
#include "lm/lm_exception.hh"
#include "lm/search_hashed.hh"
#include "lm/search_trie.hh"
#include "lm/read_arpa.hh"
#include "util/murmur_hash.hh"

#include <algorithm>
#include <functional>
#include <numeric>
#include <cmath>

namespace lm {
namespace ngram {
namespace detail {

template <class Search, class VocabularyT> const ModelType GenericModel<Search, VocabularyT>::kModelType = Search::kModelType;

template <class Search, class VocabularyT> size_t GenericModel<Search, VocabularyT>::Size(const std::vector<uint64_t> &counts, const Config &config) {
  return VocabularyT::Size(counts[0], config) + Search::Size(counts, config);
}

template <class Search, class VocabularyT> void GenericModel<Search, VocabularyT>::SetupMemory(void *base, const std::vector<uint64_t> &counts, const Config &config) {
  uint8_t *start = static_cast<uint8_t*>(base);
  size_t allocated = VocabularyT::Size(counts[0], config);
  vocab_.SetupMemory(start, allocated, counts[0], config);
  start += allocated;
  start = search_.SetupMemory(start, counts, config);
  if (static_cast<std::size_t>(start - static_cast<uint8_t*>(base)) != Size(counts, config)) UTIL_THROW(FormatLoadException, "The data structures took " << (start - static_cast<uint8_t*>(base)) << " but Size says they should take " << Size(counts, config));
}

template <class Search, class VocabularyT> GenericModel<Search, VocabularyT>::GenericModel(const char *file, const Config &config) {
  LoadLM(file, config, *this);

  // g++ prints warnings unless these are fully initialized.  
  State begin_sentence = State();
  begin_sentence.length = 1;
  begin_sentence.words[0] = vocab_.BeginSentence();
  begin_sentence.backoff[0] = search_.unigram.Lookup(begin_sentence.words[0]).backoff;
  State null_context = State();
  null_context.length = 0;
  P::Init(begin_sentence, null_context, vocab_, search_.MiddleEnd() - search_.MiddleBegin() + 2);
}

template <class Search, class VocabularyT> void GenericModel<Search, VocabularyT>::InitializeFromBinary(void *start, const Parameters &params, const Config &config, int fd) {
  SetupMemory(start, params.counts, config);
  vocab_.LoadedBinary(params.fixed.has_vocabulary, fd, config.enumerate_vocab);
  search_.LoadedBinary();
}

template <class Search, class VocabularyT> void GenericModel<Search, VocabularyT>::InitializeFromARPA(const char *file, const Config &config) {
  // Backing file is the ARPA.  Steal it so we can make the backing file the mmap output if any.  
  util::FilePiece f(backing_.file.release(), file, config.messages);
  try {
    std::vector<uint64_t> counts;
    // File counts do not include pruned trigrams that extend to quadgrams etc.   These will be fixed by search_.
    ReadARPACounts(f, counts);

    if (counts.size() > kMaxOrder) UTIL_THROW(FormatLoadException, "This model has order " << counts.size() << ".  Edit lm/max_order.hh, set kMaxOrder to at least this value, and recompile.");
    if (counts.size() < 2) UTIL_THROW(FormatLoadException, "This ngram implementation assumes at least a bigram model.");
    if (config.probing_multiplier <= 1.0) UTIL_THROW(ConfigException, "probing multiplier must be > 1.0");

    std::size_t vocab_size = VocabularyT::Size(counts[0], config);
    // Setup the binary file for writing the vocab lookup table.  The search_ is responsible for growing the binary file to its needs.  
    vocab_.SetupMemory(SetupJustVocab(config, counts.size(), vocab_size, backing_), vocab_size, counts[0], config);

    if (config.write_mmap) {
      WriteWordsWrapper wrap(config.enumerate_vocab);
      vocab_.ConfigureEnumerate(&wrap, counts[0]);
      search_.InitializeFromARPA(file, f, counts, config, vocab_, backing_);
      wrap.Write(backing_.file.get());
    } else {
      vocab_.ConfigureEnumerate(config.enumerate_vocab, counts[0]);
      search_.InitializeFromARPA(file, f, counts, config, vocab_, backing_);
    }

    if (!vocab_.SawUnk()) {
      assert(config.unknown_missing != THROW_UP);
      // Default probabilities for unknown.  
      search_.unigram.Unknown().backoff = 0.0;
      search_.unigram.Unknown().prob = config.unknown_missing_logprob;
    }
    FinishFile(config, kModelType, kVersion, counts, vocab_.UnkCountChangePadding(), backing_);
  } catch (util::Exception &e) {
    e << " Byte: " << f.Offset();
    throw;
  }
}

template <class Search, class VocabularyT> void GenericModel<Search, VocabularyT>::UpdateConfigFromBinary(int fd, const std::vector<uint64_t> &counts, Config &config) {
  util::AdvanceOrThrow(fd, VocabularyT::Size(counts[0], config));
  Search::UpdateConfigFromBinary(fd, counts, config);
}

template <class Search, class VocabularyT> FullScoreReturn GenericModel<Search, VocabularyT>::FullScore(const State &in_state, const WordIndex new_word, State &out_state) const {
  FullScoreReturn ret = ScoreExceptBackoff(in_state.words, in_state.words + in_state.length, new_word, out_state);
  for (const float *i = in_state.backoff + ret.ngram_length - 1; i < in_state.backoff + in_state.length; ++i) {
    ret.prob += *i;
  }
  return ret;
}

template <class Search, class VocabularyT> FullScoreReturn GenericModel<Search, VocabularyT>::FullScoreForgotState(const WordIndex *context_rbegin, const WordIndex *context_rend, const WordIndex new_word, State &out_state) const {
  context_rend = std::min(context_rend, context_rbegin + P::Order() - 1);
  FullScoreReturn ret = ScoreExceptBackoff(context_rbegin, context_rend, new_word, out_state);

  // Add the backoff weights for n-grams of order start to (context_rend - context_rbegin).
  unsigned char start = ret.ngram_length;
  if (context_rend - context_rbegin < static_cast<std::ptrdiff_t>(start)) return ret;
  if (start <= 1) {
    ret.prob += search_.unigram.Lookup(*context_rbegin).backoff;
    start = 2;
  }
  typename Search::Node node;
  if (!search_.FastMakeNode(context_rbegin, context_rbegin + start - 1, node)) {
    return ret;
  }
  float backoff;
  // i is the order of the backoff we're looking for.
  typename Search::MiddleIter mid_iter = search_.MiddleBegin() + start - 2;
  for (const WordIndex *i = context_rbegin + start - 1; i < context_rend; ++i, ++mid_iter) {
    if (!search_.LookupMiddleNoProb(*mid_iter, *i, backoff, node)) break;
    ret.prob += backoff;
  }
  return ret;
}

template <class Search, class VocabularyT> void GenericModel<Search, VocabularyT>::GetState(const WordIndex *context_rbegin, const WordIndex *context_rend, State &out_state) const {
  // Generate a state from context.  
  context_rend = std::min(context_rend, context_rbegin + P::Order() - 1);
  if (context_rend == context_rbegin) {
    out_state.length = 0;
    return;
  }
  FullScoreReturn ignored;
  typename Search::Node node;
  search_.LookupUnigram(*context_rbegin, out_state.backoff[0], node, ignored);
  out_state.length = HasExtension(out_state.backoff[0]) ? 1 : 0;
  float *backoff_out = out_state.backoff + 1;
  typename Search::MiddleIter mid(search_.MiddleBegin());
  for (const WordIndex *i = context_rbegin + 1; i < context_rend; ++i, ++backoff_out, ++mid) {
    if (!search_.LookupMiddleNoProb(*mid, *i, *backoff_out, node)) {
      std::copy(context_rbegin, context_rbegin + out_state.length, out_state.words);
      return;
    }
    if (HasExtension(*backoff_out)) out_state.length = i - context_rbegin + 1;
  }
  std::copy(context_rbegin, context_rbegin + out_state.length, out_state.words);
}

template <class Search, class VocabularyT> FullScoreReturn GenericModel<Search, VocabularyT>::ExtendLeft(
    const WordIndex *add_rbegin, const WordIndex *add_rend,
    const float *backoff_in,
    uint64_t extend_pointer,
    unsigned char extend_length,
    float *backoff_out,
    unsigned char &next_use) const {
  FullScoreReturn ret;
  float subtract_me;
  typename Search::Node node(search_.Unpack(extend_pointer, extend_length, subtract_me));
  ret.prob = subtract_me;
  ret.ngram_length = extend_length;
  next_use = 0;
  // If this function is called, then it does depend on left words.   
  ret.independent_left = false;
  ret.extend_left = extend_pointer;
  typename Search::MiddleIter mid_iter(search_.MiddleBegin() + extend_length - 1);
  const WordIndex *i = add_rbegin;
  for (; ; ++i, ++backoff_out, ++mid_iter) {
    if (i == add_rend) {
      // Ran out of words.
      for (const float *b = backoff_in + ret.ngram_length - extend_length; b < backoff_in + (add_rend - add_rbegin); ++b) ret.prob += *b;
      ret.prob -= subtract_me;
      return ret;
    }
    if (mid_iter == search_.MiddleEnd()) break;
    if (ret.independent_left || !search_.LookupMiddle(*mid_iter, *i, *backoff_out, node, ret)) {
      // Didn't match a word. 
      ret.independent_left = true;
      for (const float *b = backoff_in + ret.ngram_length - extend_length; b < backoff_in + (add_rend - add_rbegin); ++b) ret.prob += *b;
      ret.prob -= subtract_me;
      return ret;
    }
    ret.ngram_length = mid_iter - search_.MiddleBegin() + 2;
    if (HasExtension(*backoff_out)) next_use = i - add_rbegin + 1;
  }

  if (ret.independent_left || !search_.LookupLongest(*i, ret.prob, node)) {
    // The last backoff weight, for Order() - 1.
    ret.prob += backoff_in[i - add_rbegin];
  } else {
    ret.ngram_length = P::Order();
  }
  ret.independent_left = true;
  ret.prob -= subtract_me;
  return ret;
}

namespace {
// Do a paraonoid copy of history, assuming new_word has already been copied
// (hence the -1).  out_state.length could be zero so I avoided using
// std::copy.   
void CopyRemainingHistory(const WordIndex *from, State &out_state) {
  WordIndex *out = out_state.words + 1;
  const WordIndex *in_end = from + static_cast<ptrdiff_t>(out_state.length) - 1;
  for (const WordIndex *in = from; in < in_end; ++in, ++out) *out = *in;
}
} // namespace

/* Ugly optimized function.  Produce a score excluding backoff.  
 * The search goes in increasing order of ngram length.  
 * Context goes backward, so context_begin is the word immediately preceeding
 * new_word.  
 */
template <class Search, class VocabularyT> FullScoreReturn GenericModel<Search, VocabularyT>::ScoreExceptBackoff(
    const WordIndex *context_rbegin,
    const WordIndex *context_rend,
    const WordIndex new_word,
    State &out_state) const {
  FullScoreReturn ret;
  // ret.ngram_length contains the last known non-blank ngram length.  
  ret.ngram_length = 1;

  float *backoff_out(out_state.backoff);
  typename Search::Node node;
  search_.LookupUnigram(new_word, *backoff_out, node, ret);
  // This is the length of the context that should be used for continuation to the right.  
  out_state.length = HasExtension(*backoff_out) ? 1 : 0;
  // We'll write the word anyway since it will probably be used and does no harm being there.  
  out_state.words[0] = new_word;
  if (context_rbegin == context_rend) return ret;
  ++backoff_out;

  // Ok start by looking up the bigram.
  const WordIndex *hist_iter = context_rbegin;
  typename Search::MiddleIter mid_iter(search_.MiddleBegin());
  for (; ; ++mid_iter, ++hist_iter, ++backoff_out) {
    if (hist_iter == context_rend) {
      // Ran out of history.  Typically no backoff, but this could be a blank.  
      CopyRemainingHistory(context_rbegin, out_state);
      // ret.prob was already set.
      return ret;
    }

    if (mid_iter == search_.MiddleEnd()) break;

    if (ret.independent_left || !search_.LookupMiddle(*mid_iter, *hist_iter, *backoff_out, node, ret)) {
      // Didn't find an ngram using hist_iter.  
      CopyRemainingHistory(context_rbegin, out_state);
      // ret.prob was already set.
      ret.independent_left = true;
      return ret;
    }
    ret.ngram_length = hist_iter - context_rbegin + 2;
    if (HasExtension(*backoff_out)) {
      out_state.length = ret.ngram_length;
    }
  }

  // It passed every lookup in search_.middle.  All that's left is to check search_.longest.  
  if (!ret.independent_left && search_.LookupLongest(*hist_iter, ret.prob, node)) {
    // It's an P::Order()-gram.  
    // There is no blank in longest_.
    ret.ngram_length = P::Order();
  }
  // This handles (N-1)-grams and N-grams.  
  CopyRemainingHistory(context_rbegin, out_state);
  ret.independent_left = true;
  return ret;
}

template class GenericModel<ProbingHashedSearch, ProbingVocabulary>;  // HASH_PROBING
template class GenericModel<trie::TrieSearch<DontQuantize, trie::DontBhiksha>, SortedVocabulary>; // TRIE_SORTED
template class GenericModel<trie::TrieSearch<DontQuantize, trie::ArrayBhiksha>, SortedVocabulary>;
template class GenericModel<trie::TrieSearch<SeparatelyQuantize, trie::DontBhiksha>, SortedVocabulary>; // TRIE_SORTED_QUANT
template class GenericModel<trie::TrieSearch<SeparatelyQuantize, trie::ArrayBhiksha>, SortedVocabulary>;

} // namespace detail
} // namespace ngram
} // namespace lm

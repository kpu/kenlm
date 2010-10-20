#include "lm/ngram.hh"

#include "lm/exception.hh"
#include "lm/read_arpa.hh"
#include "util/file_piece.hh"
#include "util/joint_sort.hh"
#include "util/murmur_hash.hh"
#include "util/probing_hash_table.hh"

#include <algorithm>
#include <functional>
#include <numeric>
#include <limits>
#include <string>

#include <cmath>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace lm {
namespace ngram {

namespace {

/* All of the entropy is in low order bits and boost::hash does poorly with
 * these.  Odd numbers near 2^64 chosen by mashing on the keyboard.  There is a
 * stable point: 0.  But 0 is <unk> which won't be queried here anyway.  
 */
inline uint64_t CombineWordHash(uint64_t current, const WordIndex next) {
  uint64_t ret = (current * 8978948897894561157ULL) ^ (static_cast<uint64_t>(next) * 17894857484156487943ULL);
  return ret;
}

uint64_t ChainedWordHash(const WordIndex *word, const WordIndex *word_end) {
  if (word == word_end) return 0;
  uint64_t current = static_cast<uint64_t>(*word);
  for (++word; word != word_end; ++word) {
    current = CombineWordHash(current, *word);
  }
  return current;
}

template <class Voc, class Store> void ReadNGrams(util::FilePiece &f, const unsigned int n, const size_t count, const Voc &vocab, Store &store) {
  ReadNGramHeader(f, n);

  // vocab ids of words in reverse order
  WordIndex vocab_ids[n];
  typename Store::Packing::Value value;
  for (size_t i = 0; i < count; ++i) {
    ReadNGram(f, n, vocab, vocab_ids, value);
    uint64_t key = ChainedWordHash(vocab_ids, vocab_ids + n);
    store.Insert(Store::Packing::Make(key, value));
  }

  if (f.ReadLine().size()) UTIL_THROW(FormatLoadException, "Expected blank line after " << n << "-grams at byte " << f.Offset());
  store.FinishedInserting();
}

} // namespace
namespace detail {

template <class Search, class VocabularyT> void GenericModel<Search, VocabularyT>::InitializeFromARPA(void *start, const Parameters &params, const Config &config, util::FilePiece &f) {
  SetupMemory(start, params.counts, config);
  // Read the unigrams.
  // TODO: unkludge &unigram_.Lookup(0).
  Read1Grams(f, params.counts[0], vocab_, &unigram_.Lookup(0));
  if (!vocab_.SawUnk()) {
    switch(config.unknown_missing) {
      case Config::THROW_UP:
        {
          SpecialWordMissingException e("<unk>");
          e << " and configuration was set to throw if unknown is missing";
          throw e;
        }
      case Config::COMPLAIN:
        if (config.messages) *config.messages << "Language model is missing <unk>.  Substituting probability " << config.unknown_missing_prob << "." << std::endl; 
        // There's no break;.  This is by design.  
      case Config::SILENT:
        // Default probabilities for unknown.  
        unigram_.Lookup(0).backoff = 0.0;
        unigram_.Lookup(0).prob = config.unknown_missing_prob;
        break;
    }
  }
  
  // Read the n-grams.
  for (unsigned int n = 2; n < params.counts.size(); ++n) {
    ReadNGrams(f, n, params.counts[n-1], vocab_, middle_[n-2]);
  }
  ReadNGrams(f, params.counts.size(), params.counts[params.counts.size() - 1], vocab_, longest_);
  if (std::fabs(unigram_.Lookup(0).backoff) > 0.0000001) UTIL_THROW(FormatLoadException, "Backoff for unknown word should be zero, but was given as " << unigram_.Lookup(0).backoff);
}

template <class Search, class VocabularyT> FullScoreReturn GenericModel<Search, VocabularyT>::FullScore(const State &in_state, const WordIndex new_word, State &out_state) const {
  unsigned char backoff_start;
  FullScoreReturn ret = ScoreExceptBackoff(in_state.history_, in_state.history_ + in_state.valid_length_, new_word, backoff_start, out_state);
  if (backoff_start - 1 < in_state.valid_length_) {
    ret.prob = std::accumulate(in_state.backoff_ + backoff_start - 1, in_state.backoff_ + in_state.valid_length_, ret.prob);
  }
  return ret;
}

template <class Search, class VocabularyT> FullScoreReturn GenericModel<Search, VocabularyT>::FullScoreForgotState(const WordIndex *context_rbegin, const WordIndex *context_rend, const WordIndex new_word, State &out_state) const {
  unsigned char backoff_start;
  context_rend = std::min(context_rend, context_rbegin + P::Order() - 1);
  FullScoreReturn ret = ScoreExceptBackoff(context_rbegin, context_rend, new_word, backoff_start, out_state);
  ret.prob += SlowBackoffLookup(context_rbegin, context_rend, backoff_start);
  return ret;
}

template <class Search, class VocabularyT> void GenericModel<Search, VocabularyT>::GetState(const WordIndex *context_rbegin, const WordIndex *context_rend, State &out_state) const {
  context_rend = std::min(context_rend, context_rbegin + P::Order() - 1);
  if (context_rend == context_rbegin || *context_rbegin == 0) {
    out_state.valid_length_ = 0;
    return;
  }
  out_state.backoff_[0] = unigram_.Lookup(*context_rbegin).backoff;
  float *backoff_out = out_state.backoff_ + 1;
  uint64_t lookup_hash = static_cast<uint64_t>(*context_rbegin);
  const WordIndex *i = context_rbegin + 1;
  typename Middle::ConstIterator found;
  for (; i < context_rend; ++i, ++backoff_out) {
    lookup_hash = CombineWordHash(lookup_hash, *i);
    if (!middle_[i - context_rbegin - 1].Find(lookup_hash, found)) {
      out_state.valid_length_ = i - context_rbegin;
      std::copy(context_rbegin, i, out_state.history_);
      return;
    }
    *backoff_out = found->GetValue().backoff;
  }
  std::copy(context_rbegin, context_rend, out_state.history_);
  out_state.valid_length_ = static_cast<unsigned char>(context_rend - context_rbegin);
}

template <class Search, class VocabularyT> float GenericModel<Search, VocabularyT>::SlowBackoffLookup(
    const WordIndex *const context_rbegin, const WordIndex *const context_rend, unsigned char start) const {
  // Add the backoff weights for n-grams of order start to (context_rend - context_rbegin).
  if (context_rend - context_rbegin < static_cast<std::ptrdiff_t>(start)) return 0.0;
  float ret = 0.0;
  if (start == 1) {
    ret += unigram_.Lookup(*context_rbegin).backoff;
    start = 2;
  }
  uint64_t lookup_hash = static_cast<uint64_t>(*context_rbegin);
  const WordIndex *i;
  for (i = context_rbegin + 1; i < context_rbegin + start - 1; ++i) {
    lookup_hash = CombineWordHash(lookup_hash, *i);
  }
  typename Middle::ConstIterator found;
  // i is the order of the backoff we're looking for.
  for (; i < context_rend; ++i) {
    lookup_hash = CombineWordHash(lookup_hash, *i);
    if (!middle_[i - context_rbegin - 1].Find(lookup_hash, found)) break;
    ret += found->GetValue().backoff;
  }
  return ret;
}

/* Ugly optimized function.  Produce a score excluding backoff.  
 * The search goes in increasing order of ngram length.  
 * Context goes backward, so context_begin is the word immediately preceeding
 * new_word.  
 */
template <class Search, class VocabularyT> FullScoreReturn GenericModel<Search, VocabularyT>::ScoreExceptBackoff(
    const WordIndex *context_rbegin,
    const WordIndex *context_rend,
    const WordIndex new_word,
    unsigned char &backoff_start,
    State &out_state) const {
  FullScoreReturn ret;
  const ProbBackoff &unigram = unigram_.Lookup(new_word);
  if (new_word == 0) {
    ret.ngram_length = out_state.valid_length_ = 0;
    // All of backoff.  
    backoff_start = 1;
    ret.prob = unigram.prob;
    return ret;
  }
  float *backoff_out(out_state.backoff_);
  *backoff_out = unigram.backoff;
  ret.prob = unigram.prob;
  out_state.history_[0] = new_word;
  if (context_rbegin == context_rend) {
    ret.ngram_length = out_state.valid_length_ = 1;
    // No backoff because we don't have the history for it.  
    backoff_start = P::Order();
    return ret;
  }
  ++backoff_out;

  // Ok now we now that the bigram contains known words.  Start by looking it up.

  uint64_t lookup_hash = static_cast<uint64_t>(new_word);
  const WordIndex *hist_iter = context_rbegin;
  typename std::vector<Middle>::const_iterator mid_iter = middle_.begin();
  for (; ; ++mid_iter, ++hist_iter, ++backoff_out) {
    if (hist_iter == context_rend) {
      // Ran out of history.  No backoff.  
      backoff_start = P::Order();
      std::copy(context_rbegin, context_rend, out_state.history_ + 1);
      ret.ngram_length = out_state.valid_length_ = (context_rend - context_rbegin) + 1;
      // ret.prob was already set.
      return ret;
    }
    lookup_hash = CombineWordHash(lookup_hash, *hist_iter);
    if (mid_iter == middle_.end()) break;
    typename Middle::ConstIterator found;
    if (!mid_iter->Find(lookup_hash, found)) {
      // Didn't find an ngram using hist_iter.  
      // The history used in the found n-gram is [context_rbegin, hist_iter).  
      std::copy(context_rbegin, hist_iter, out_state.history_ + 1);
      // Therefore, we found a (hist_iter - context_rbegin + 1)-gram including the last word.  
      ret.ngram_length = out_state.valid_length_ = (hist_iter - context_rbegin) + 1;
      backoff_start = mid_iter - middle_.begin() + 1;
      // ret.prob was already set.  
      return ret;
    }
    *backoff_out = found->GetValue().backoff;
    ret.prob = found->GetValue().prob;
  }

  // It passed every lookup in middle_.  That means it's at least a (P::Order() - 1)-gram. 
  // All that's left is to check longest_.  
  
  typename Longest::ConstIterator found;
  if (!longest_.Find(lookup_hash, found)) {
    // It's an (P::Order()-1)-gram
    std::copy(context_rbegin, context_rbegin + P::Order() - 2, out_state.history_ + 1);
    ret.ngram_length = out_state.valid_length_ = P::Order() - 1;
    backoff_start = P::Order() - 1;
    // ret.prob was already set.  
    return ret;
  }
  // It's an P::Order()-gram
  // out_state.valid_length_ is still P::Order() - 1 because the next lookup will only need that much.
  std::copy(context_rbegin, context_rbegin + P::Order() - 2, out_state.history_ + 1);
  out_state.valid_length_ = P::Order() - 1;
  ret.ngram_length = P::Order();
  ret.prob = found->GetValue().prob;
  backoff_start = P::Order();
  return ret;
}

} // namespace detail
} // namespace ngram
} // namespace lm

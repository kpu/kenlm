#include "lm/wrappers/nplm.hh"

#include <algorithm>

namespace lm {
namespace np {

Vocabulary::Vocabulary(const nplm::vocabulary &vocab) 
  : base::Vocabulary(vocab.lookup_word("<s>"), vocab.lookup_word("</s>"), vocab.lookup_word("<unk>")),
    vocab_(vocab) {}

Vocabulary::~Vocabulary() {}

WordIndex Vocabulary::Index(const std::string &str) const {
  return vocab_.lookup_word(str);
}

Model::Model(const std::string &file) : backend_(file), vocab_(backend_.get_vocabulary()) {
  State begin_sentence, null_context;
  std::fill(begin_sentence.words, begin_sentence.words + NPLM_MAX_ORDER - 1, backend_.lookup_word("<s>"));
  null_word_ = backend_.lookup_word("<null>");
  std::fill(null_context.words, null_context.words + NPLM_MAX_ORDER - 1, null_word_);

  Init(begin_sentence, null_context, vocab_, backend_.get_order());
}

FullScoreReturn Model::FullScore(const State &from, const WordIndex new_word, State &out_state) const {
  // State is in natural word order.
  FullScoreReturn ret;
  for (int i = 0; i < backend_.get_order() - 1; ++i) {
    backend_.staging_ngram()(i) = from.words[i];
  }
  backend_.staging_ngram()(backend_.get_order() - 1) = new_word;
  ret.prob = backend_.lookup_from_staging();
  // Always say full order.
  ret.ngram_length = backend_.get_order();
  // Shift everything down by one.
  memcpy(out_state.words, from.words + 1, sizeof(WordIndex) * (backend_.get_order() - 2));
  out_state.words[backend_.get_order() - 2] = new_word;
  // Fill in trailing words with zeros so state comparison works.
  memset(out_state.words + backend_.get_order() - 1, 0, sizeof(WordIndex) * (NPLM_MAX_ORDER - backend_.get_order()));
}

// TODO: optimize with direct call?
FullScoreReturn Model::FullScoreForgotState(const WordIndex *context_rbegin, const WordIndex *context_rend, const WordIndex new_word, State &out_state) const {
  // State is in natural word order.  The API here specifies reverse order.
  std::size_t state_length = std::min<std::size_t>(backend_.get_order() - 1, context_rend - context_rbegin);
  State state;
  // Pad with null words.
  for (lm::WordIndex *i = state.words; i < state.words + backend_.get_order() - 1 - state_length; ++i) {
    *i = null_word_;
  }
  // Put new words at the end.
  std::reverse_copy(context_rbegin, context_rbegin + state_length, state.words + backend_.get_order() - 1 - state_length);
  return FullScore(state, new_word, out_state);
}

} // namespace np
} // namespace lm

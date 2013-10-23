#include "lm/wrappers/npln.hh"

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

Model::Model(const std::string &file) : backend_(new nplm::neuralLM) {
  backend_->read(file);
  State begin_sentence, null_context;
  std::fill(begin_sentence.words, begin_sentence.words + NPLM_MAX_ORDER - 1, backend_->lookup_word("<s>"));
  std::fill(null_context.words, null_context.words + NPLM_MAX_ORDER - 1, backend_->lookup_word("<null>"));
  Init(begin_sentence, null_context, backend_->get_vocabulary(), backend_->get_order());
}

Model::~Model() {}

FullScoreReturn Model::FullScore(const State &from, const WordIndex new_word, State &out_state) {
  FullScoreReturn ret;
  ret.prob = backend_->lookup_append(from.words, new_word);
  // Always uses the full n-gram.  Maybe this should exclude null?
  ret.ngram_length = backend_->get_order();
  memcpy(out_state.words, from.words + 1, 
}

FullScoreReturn Model::FullScoreForgotState(const WordIndex *context_rbegin, const WordIndex *context_rend, const WordIndex new_word, State &out_state) {
  FullScoreReturn ret;
  WordIndex words[NPLM_MAX_ORDER];
  context_rend = context_rbegin + std::min<std::size_t>(NPLM_MAX_ORDER - 1, context_rend - context_rbegin);
  *std::reverse_copy(context_rbegin, context_rend, words) = new_word;
  ret.prob = backend_->lookup_ngram(words, context_rend - context_rbegin + 1);
  *out_state.words[0] = new_word;
  std::copy(context
}

} // namespace np
} // namespace lm

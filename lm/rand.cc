#include "lm/rand.hh"
#include "RandLMFile.h"
#include "RandLMTypes.h"
#include "RandLM.h"

namespace lm {
namespace rand {

lm::WordIndex Vocabulary::Index(const std::string &str) const {
  return inner_->getWordID(str);
}

void Vocabulary::FinishedLoading() {
  SetSpecial(randlm::Vocab::kBOSWordID, Index(randlm::Vocab::kEOS), randlm::Vocab::kOOVWordID);
}

Model::Model(const char *file_name, unsigned int order, EnumerateVocab *enumerate) {
  int cache_MB = 50;
  inner_.reset(randlm::RandLM::initRandLM(file_name, order, cache_MB));
  inner_->initThreadSpecificData();
  vocab_.SetVocab(inner_->getVocab());
  vocab_.FinishedLoading();
  State begin, null_context;
  begin.valid_length_ = 1;
  begin.history_[0] = vocab_.BeginSentence();
  null_context.valid_length_ = 0;
  Init(begin, null_context, vocab_, order);

  if (enumerate) {
    for(std::map<randlm::Word, randlm::WordID>::const_iterator vIter = inner->vocabStart(); vIter != inner->vocabEnd(); ++vIter) {
      enumerate->Add(vIter->second, vIter->first);
    }
  }
}

Model::~Model() {}

FullScoreReturn Model::FullScore(const State &in_state, const WordIndex new_word, State &out_state) const {
  uint32_t ngram[kMaxOrder];
  std::copy(in_state.history_, in_state.history_ + in_state.valid_length_, ngram);
  ngram[in_state.valid_length_] = new_word;
  int found = 0;
  FullScoreReturn ret;
  ret.prob = inner_->getProb(&ngram[0], in_state.valid_length_ + 1, &found, NULL);
  ret.ngram_length = found;
  if (ret.ngram_length >= Order()) {
    out_state.valid_length_ = Order() - 1;
    std::copy(ngram + 1, ngram + Order(), out_state.history_);
  } else {
    const uint32_t *end = ngram + in_state.valid_length_ + 1;
    out_state.valid_length_ = ret.ngram_length;
    std::copy(end - ret.ngram_length, end, out_state.history_);
  }
  return ret;
}

} // namespace rand
} // namespace lm

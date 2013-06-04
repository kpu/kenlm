#include "lm/lm_exception.hh"
#include "lm/sri.hh"

#include <Ngram.h>
#include <Vocab.h>

#include <errno.h>

namespace lm {
namespace sri {

Vocabulary::Vocabulary() : sri_(new Vocab) {}

Vocabulary::~Vocabulary() {}

WordIndex Vocabulary::Index(const char *str) const {
  WordIndex ret = sri_->getIndex(str);
  // NGram wants the index of Vocab_Unknown for unknown words, but for some reason SRI returns Vocab_None here :-(.
  if (ret == Vocab_None) {
    return not_found_;
  } else {
    return ret;
  }
}

const char *Vocabulary::Word(WordIndex index) const {
  return sri_->getWord(index);
}

void Vocabulary::FinishedLoading() {
  SetSpecial(
    sri_->ssIndex(),
    sri_->seIndex(),
    sri_->unkIndex());
}

namespace {
Ngram *MakeSRIModel(const char *file_name, unsigned int ngram_length, Vocab &sri_vocab) {
  sri_vocab.unkIsWord() = true;
  std::auto_ptr<Ngram> ret(new Ngram(sri_vocab, ngram_length));
  File file(file_name, "r");
  errno = 0;
  if (!ret->read(file)) {
    UTIL_THROW(FormatLoadException, "reading file " << file_name << " with SRI failed.");
  }
  return ret.release();
}
} // namespace

Model::Model(const char *file_name, unsigned int ngram_length) : sri_(MakeSRIModel(file_name, ngram_length, *vocab_.sri_)) {
  if (!sri_->setorder()) {
    UTIL_THROW(FormatLoadException, "Can't have an SRI model with order 0.");
  }
  vocab_.FinishedLoading();
  State begin_state = State();
  begin_state.valid_length_ = 1;
  if (kMaxOrder > 1) {
    begin_state.history_[0] = vocab_.BeginSentence();
    if (kMaxOrder > 2) begin_state.history_[1] = Vocab_None;
  }
  State null_state = State();
  null_state.valid_length_ = 0;
  if (kMaxOrder > 1) null_state.history_[0] = Vocab_None;
  Init(begin_state, null_state, vocab_, sri_->setorder());
}

Model::~Model() {}

namespace {

/* Argh SRI's wordProb knows the ngram length but doesn't return it.  One more
 * reason you should use my model.  */
// TODO(stolcke): fix SRILM so I don't have to do this.   
unsigned int MatchedLength(Ngram &model, const WordIndex new_word, const SRIVocabIndex *const_history) {
  unsigned int out_length = 0;
  // This gets the length of context used, which is ngram_length - 1 unless new_word is OOV in which case it is 0.
  model.contextID(new_word, const_history, out_length);
  return out_length + 1;
}

} // namespace

FullScoreReturn Model::FullScore(const State &in_state, const WordIndex new_word, State &out_state) const {
  // If you get a compiler in this function, change SRIVocabIndex in sri.hh to match the one found in SRI's Vocab.h.
  const SRIVocabIndex *const_history;
  SRIVocabIndex local_history[Order()];
  if (in_state.valid_length_ < kMaxOrder - 1) {
    const_history = in_state.history_;
  } else {
    std::copy(in_state.history_, in_state.history_ + in_state.valid_length_, local_history);
    local_history[in_state.valid_length_] = Vocab_None;
    const_history = local_history;
  }
  FullScoreReturn ret;
  ret.ngram_length = MatchedLength(*sri_, new_word, const_history);
  out_state.history_[0] = new_word;
  out_state.valid_length_ = std::min<unsigned char>(ret.ngram_length, Order() - 1);
  std::copy(const_history, const_history + out_state.valid_length_ - 1, out_state.history_ + 1);
  if (out_state.valid_length_ < kMaxOrder - 1) {
    out_state.history_[out_state.valid_length_] = Vocab_None;
  }
  ret.prob = sri_->wordProb(new_word, const_history);
  return ret;
}

} // namespace sri
} // namespace lm

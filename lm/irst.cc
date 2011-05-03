#include "lm/irst.hh"

#include "lm/lm_exception.hh"

#include "dictionary.h"
#include "n_gram.h"
#include "lmtable.h"
#include "lmmacro.h"

#include <iostream>
#include <fstream>

namespace lm {
namespace irst {

WordIndex Vocabulary::Index(const char *str) const {
  return d_->encode(str);
}

void Vocabulary::FinishedLoading() {
  SetSpecial(Index(BOS_), Index(EOS_), d_->oovcode());
}

Model::Model(const char *file_name) {
  int model_type = getLanguageModelType(file_name);
  switch (model_type) {
    case _IRSTLM_LMMACRO:
      {
        lmmacro *lm;
        table_.reset((lm = new lmmacro()));
        lm->load(file_name);
        table_->getDict()->incflag(1);
      }
      break;
    case _IRSTLM_LMTABLE:
      {
        table_.reset(new lmtable());
        std::ifstream inp(file_name);
        bool load_with_mmap = (strlen(file_name) > 3 && !strcmp(file_name + strlen(file_name) - 3, ".mm"));
        std::cerr << "Loading with" << (load_with_mmap ? "" : "out") << " mmap" << std::endl;
        table_->load(inp, file_name, NULL, load_with_mmap);
      }
      break;
    default:
      UTIL_THROW(FormatLoadException, "Bad IRSTLM model type " << model_type);
  }
  vocab_.SetDictionary(table_->getDict());
  max_level_ = table_->maxlevel();
  table_->init_caches(max_level_>2?max_level_-1:2);
  vocab_.FinishedLoading();

  State begin, null_context;
  // Automatically pads with <s>
  begin.valid_length_ = 0;
  // Won't work with multiple <unk> chains or where <unk> has backoff but there doesn't seem to be any other way to call IRST without context...
  null_context.valid_length_ = 1;
  null_context.history_[0] = vocab_.NotFound();
  Init(begin, null_context, vocab_, max_level_);
}

Model::~Model() {
  table_->reset_mmap();
}

FullScoreReturn Model::FullScore(const State &in_state, const WordIndex new_word, State &out_state) const {
  int codes[kMaxOrder];
  size_t idx=0;
  size_t count = in_state.valid_length_ + 1;
  if (count < (size_t) (max_level_-1)) codes[idx++] = vocab_.EndSentence();
  if (count < (size_t) max_level_) codes[idx++] = vocab_.BeginSentence();
  for (unsigned char i = 0 ; i < in_state.valid_length_; i++) {
    codes[idx++] = in_state.history_[i];
  }
  codes[idx++] = new_word;


  unsigned int ilen;
  char *msp = NULL;

  FullScoreReturn ret;
  ret.prob = table_->clprob(codes, idx, NULL, NULL, &msp, &ilen);
  // TODO: check this is correct.  
  ret.ngram_length = ilen;
  if (ret.ngram_length >= max_level_) {
    out_state.valid_length_ = max_level_;
    std::copy(in_state.history_ + 1, in_state.history_ + in_state.valid_length_, out_state.history_);
  } else {
    out_state.valid_length_ = in_state.valid_length_ + 1;
    std::copy(in_state.history_, in_state.history_ + in_state.valid_length_, out_state.history_);
  }
  out_state.history_[out_state.valid_length_ - 1] = new_word;

  return ret;
}

} // namespace irst
} // namespace lm

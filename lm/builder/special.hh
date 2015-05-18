#ifndef LM_BUILDER_SPECIAL_H
#define LM_BUILDER_SPECIAL_H

#include "lm/word_index.hh"

namespace lm { namespace builder {

class SpecialVocab {
  public:
    SpecialVocab(WordIndex bos, WordIndex eos) : bos_(bos), eos_(eos) {}

    bool IsSpecial(WordIndex word) const {
      return word == kUNK || word == bos_ || word == eos_;
    }

    WordIndex UNK() const { return kUNK; }
    WordIndex BOS() const { return bos_; }
    WordIndex EOS() const { return eos_; }

  private:
    WordIndex bos_;
    WordIndex eos_;
};

}} // namespaces

#endif // LM_BUILDER_SPECIAL_H

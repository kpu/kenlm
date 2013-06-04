#ifndef LM_RAND__
#define LM_RAND__

#include "lm/facade.hh"
#include "lm/virtual_interface.hh"

#include <memory>
#include <string>

#include <stdint.h>

namespace randlm {
class RandLM;
class Vocab;
} // namespace randlm

namespace lm {
class EnumerateVocab;
namespace rand {

const unsigned int kMaxOrder = 6;

class State {
  public:
    uint32_t history_[kMaxOrder - 1];
    unsigned char valid_length_;
};

class Vocabulary : public base::Vocabulary {
  public:
    Vocabulary() {}

    WordIndex Index(const StringPiece &str) const {
      std::string temp(str.data(), str.length());
      return Index(temp);
    }
    WordIndex Index(const std::string &str) const;
    WordIndex Index(const char *str) const {
      std::string temp(str);
      return Index(temp);
    }

  private:
    friend class Model;
    void SetVocab(randlm::Vocab *inner) { inner_ = inner; }

    void FinishedLoading();

    mutable randlm::Vocab *inner_;
};

class Model : public base::ModelFacade<Model, State, Vocabulary> {
  public:
    Model(const char *file_name, unsigned int order, EnumerateVocab *enumerate = NULL);

    ~Model();

    FullScoreReturn FullScore(const State &in_state, const WordIndex new_word, State &out_state) const;

  private:
    mutable std::auto_ptr<randlm::RandLM> inner_;
    Vocabulary vocab_;
};

} // namespace rand
} // namespace lm

#endif // LM_RAND__

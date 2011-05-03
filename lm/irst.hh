#ifndef LM_IRST__
#define LM_IRST__

#include "lm/facade.hh"
#include "lm/virtual_interface.hh"

#include <memory>
#include <string>

class lmtable;  // irst lm table
class lmmacro;  // irst lm for macro tags
class ngram;
class dictionary;

namespace lm {
namespace irst {

const unsigned int kMaxOrder = 5;

class State {
  public:
    // You shouldn't need to touch these, but they're public so State will be a POD.
    // If valid_length_ < kMaxOrder - 1 then history_[valid_length_] == Vocab_None.
    int history_[kMaxOrder - 1];
    unsigned char valid_length_;
};

class Vocabulary : public base::Vocabulary {
  public:
    Vocabulary() {}

    WordIndex Index(const StringPiece &str) const {
      std::string temp(str.data(), str.length());
      return Index(temp.c_str());
    }
    WordIndex Index(const std::string &str) const {
      return Index(str.c_str());
    }
    WordIndex Index(const char *str) const;

  private:
    friend class Model;
    void SetDictionary(dictionary *d) { d_ = d; }

    void FinishedLoading();

    mutable dictionary *d_;
};

class Model : public base::ModelFacade<Model, State, Vocabulary> {
  public:
    explicit Model(const char *file_name);

    ~Model();

    FullScoreReturn FullScore(const State &in_state, const WordIndex new_word, State &out_state) const;

  private:
    Vocabulary vocab_;

    mutable std::auto_ptr<lmtable> table_;

    unsigned char max_level_;
};

} // namespace irst
} // namespace lm

#endif // LM_IRST__

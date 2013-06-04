#ifndef LM_SRI__
#define LM_SRI__

#include "lm/facade.hh"
#include "util/murmur_hash.hh"

#include <cmath>
#include <exception>
#include <memory>

class Ngram;
class Vocab;

/* The ngram length reported uses some random API I found and may be wrong.
 *
 * See ngram, which should return equivalent results.
 */

namespace lm {
namespace sri {

static const unsigned int kMaxOrder = 6;

/* This should match VocabIndex found in SRI's Vocab.h
 * The reason I define this here independently is that SRI's headers
 * pollute and increase compile time.
 * It's difficult to extract this from their header and anyway would
 * break packaging.
 * If these differ there will be a compiler error in ActuallyCall.
 */
typedef unsigned int SRIVocabIndex;

class State {
  public:
    // You shouldn't need to touch these, but they're public so State will be a POD.
    // If valid_length_ < kMaxOrder - 1 then history_[valid_length_] == Vocab_None.
    SRIVocabIndex history_[kMaxOrder - 1];
    unsigned char valid_length_;
};

inline bool operator==(const State &left, const State &right) {
  if (left.valid_length_ != right.valid_length_) {
    return false;
  }
  for (const SRIVocabIndex *l = left.history_, *r = right.history_;
      l != left.history_ + left.valid_length_;
      ++l, ++r) {
    if (*l != *r) return false;
  }
  return true;
}

inline size_t hash_value(const State &state) {
  return util::MurmurHashNative(&state.history_, sizeof(SRIVocabIndex) * state.valid_length_);
}

class Vocabulary : public base::Vocabulary {
  public:
    Vocabulary();

    ~Vocabulary();

    WordIndex Index(const StringPiece &str) const {
      std::string temp(str.data(), str.length());
      return Index(temp.c_str());
    }
    WordIndex Index(const std::string &str) const {
      return Index(str.c_str());
    }
    WordIndex Index(const char *str) const;

    const char *Word(WordIndex index) const;

  private:
    friend class Model;
    void FinishedLoading();

    // The parent class isn't copyable so auto_ptr is the same as scoped_ptr
    // but without the boost dependence.  
    mutable std::auto_ptr<Vocab> sri_;
};

class Model : public base::ModelFacade<Model, State, Vocabulary> {
  public:
    Model(const char *file_name, unsigned int ngram_length);

    ~Model();

    FullScoreReturn FullScore(const State &in_state, const WordIndex new_word, State &out_state) const;

  private:
    Vocabulary vocab_;

    mutable std::auto_ptr<Ngram> sri_;
};

} // namespace sri
} // namespace lm

#endif // LM_SRI__

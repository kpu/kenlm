#ifndef LM_WRAPPER_NPLM__
#define LM_WRAPPER_NPLM__

#include "lm/facade.hh"
#include "lm/max_order.hh"
#include "util/string_piece.hh"

#include "neuralLM.h"

/* Wrapper to NPLM "by Ashish Vaswani, with contributions from David Chiang
 * and Victoria Fossum."  
 * http://nlg.isi.edu/software/nplm/
 */

namespace nplm {
class vocabulary;
} // namespace nplm

namespace lm {
namespace np {

class Vocabulary : public base::Vocabulary {
  public:
    Vocabulary(const nplm::vocabulary &vocab);

    ~Vocabulary();

    WordIndex Index(const std::string &str) const;

    // TODO: lobby them to support StringPiece
    WordIndex Index(const StringPiece &str) const {
      return Index(std::string(str.data(), str.size()));
    }

  private:
    const nplm::vocabulary &vocab_;
};

// Sorry for imposing my limitations on your code.
#define NPLM_MAX_ORDER 7

struct State {
  WordIndex words[NPLM_MAX_ORDER - 1];
};

// Not threadsafe!  Make a copy for each thread.
class Model : public lm::base::ModelFacade<Model, State, Vocabulary> {
  private:
    typedef lm::base::ModelFacade<Model, State, Vocabulary> P;

  public:
    // Does this look like an NPLM?
    static bool Recognize(const std::string &file);

    explicit Model(const std::string &file);

    FullScoreReturn FullScore(const State &from, const WordIndex new_word, State &out_state) const;

    FullScoreReturn FullScoreForgotState(const WordIndex *context_rbegin, const WordIndex *context_rend, const WordIndex new_word, State &out_state) const;

  private:
    // Sigh.
    mutable nplm::neuralLM backend_;
    Vocabulary vocab_;

    lm::WordIndex null_word_;
};

} // namespace np
} // namespace lm

#endif // LM_WRAPPER_NPLM__

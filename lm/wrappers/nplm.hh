#ifndef LM_WRAPPER_NPLM__
#define LM_WRAPPER_NPLM__

#include "lm/base.hh"
#include "lm/max_order.hh"
#include "util/string_piece.hh"

// NPLM uses Boost anyway.
#include <boost/scoped_ptr.hpp>

/* Wrapper to NPLM "by Ashish Vaswani, with contributions from David Chiang
 * and Victoria Fossum."  
 * http://nlg.isi.edu/software/nplm/
 */

namespace nplm {
class vocabulary;
class neuralLM;
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

class Model : public lm::base::ModelFacade<Model, State, Vocabulary> {
  private:
    typedef lm::base::ModelFacade<Model, State, Vocabulary> P;

  public:
    Model(const std::string &file);

    // for scoped_ptr destructor.
    ~Model();

    FullScoreReturn FullScore(const State &from, const WordIndex new_word, State &out_state);

    FullScoreReturn FullScoreForgotState(const WordIndex *context_rbegin, const WordIndex *context_rend, const WordIndex new_word,State &out_state);

  private:
    boost::scoped_ptr<nplm::neuralLM> backend_;
};

} // namespace np
} // namespace lm

#endif // LM_WRAPPER_NPLM__

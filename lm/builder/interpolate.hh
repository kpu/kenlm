#ifndef LM_BUILDER_INTERPOLATE__
#define LM_BUILDER_INTERPOLATE__

#include "lm/builder/multi_stream.hh"
#include "lm/word_index.hh"

#include <stdint.h>

namespace lm { namespace builder {

class Binarize;
 
/* Interpolate step.  
 * Input: suffix sorted n-grams with (p_uninterpolated, gamma) from
 * InitialProbabilities.
 * Output: suffix sorted n-grams with complete probability
 */
class Interpolate {
  public:
    explicit Interpolate(uint64_t unigram_count, Binarize &binarize, const ChainPositions &backoffs);

    void Run(const ChainPositions &positions);

  private:
    float uniform_prob_;
    Binarize &binarize_;
    ChainPositions backoffs_;
};

}} // namespaces
#endif // LM_BUILDER_INTERPOLATE__

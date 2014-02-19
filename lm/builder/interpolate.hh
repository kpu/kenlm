#ifndef LM_BUILDER_INTERPOLATE_H
#define LM_BUILDER_INTERPOLATE_H

#include <stdint.h>

#include "lm/builder/multi_stream.hh"

namespace lm { namespace builder {
 
/* Interpolate step.  
 * Input: suffix sorted n-grams with (p_uninterpolated, gamma) from
 * InitialProbabilities.
 * Output: suffix sorted n-grams with complete probability
 */
class Interpolate {
  public:
    // Normally the unigram count-1 (since p(<s>) = 0) but might be larger to
    // set a consistent vocabulary size.
    explicit Interpolate(uint64_t vocab_size, const ChainPositions &backoffs);

    void Run(const ChainPositions &positions);

  private:
    float uniform_prob_;
    ChainPositions backoffs_;
};

}} // namespaces
#endif // LM_BUILDER_INTERPOLATE_H

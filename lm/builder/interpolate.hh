#ifndef LM_BUILDER_INTERPOLATE__
#define LM_BUILDER_INTERPOLATE__

#include <stdint.h>

namespace lm { namespace builder {
 
class ChainPositions;

/* Interpolate step.  
 * Input: suffix sorted n-grams with (p_uninterpolated, gamma) from
 * InitialProbabilities.
 * Output: suffix sorted n-grams with (p_interpolated, p_lower) for Backoff.
 */
class Interpolate {
  public:
    explicit Interpolate(uint64_t unigram_count);

    void Run(const ChainPositions &positions);

  private:
    float uniform_prob_;
};

}} // namespaces
#endif // LM_BUILDER_INTERPOLATE__

#ifndef LM_BUILDER_INTERPOLATE__
#define LM_BUILDER_INTERPOLATE__

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
    explicit Interpolate(uint64_t unigram_count, const ChainPositions &backoffs,
                         const std::vector<uint64_t> &prune_thresholds_);

    void Run(const ChainPositions &positions);

  private:
    float uniform_prob_;
    ChainPositions backoffs_;
    const std::vector<uint64_t> prune_thresholds_;
};

}} // namespaces
#endif // LM_BUILDER_INTERPOLATE__

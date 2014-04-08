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
    // Normally vocab_size is the unigram count-1 (since p(<s>) = 0) but might
    // be larger when the user specifies a consistent vocabulary size.
    explicit Interpolate(uint64_t vocab_size, const ChainPositions &backoffs, const std::vector<uint64_t> &prune_thresholds);

    void Run(const ChainPositions &positions);

  private:
    float uniform_prob_;
    ChainPositions backoffs_;
    const std::vector<uint64_t> prune_thresholds_;
};

}} // namespaces
#endif // LM_BUILDER_INTERPOLATE_H

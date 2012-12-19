#ifndef LM_BUILDER_INITIAL_PROBABILITIES__
#define LM_BUILDER_INITIAL_PROBABILITIES__

#include "lm/builder/discount.hh"
#include "util/stream/config.hh"

#include <vector>

namespace lm {
namespace builder {
class Chains;

struct InitialProbabilitiesConfig {
  // These should be small buffers to keep the adder from getting too far ahead
  util::stream::ChainConfig adder_in;
  util::stream::ChainConfig adder_out;
  // SRILM doesn't normally interpolate unigrams.  
  bool interpolate_unigrams;
};

/* Compute initial (uninterpolated) probabilities
 * Input: context sorted adjusted counts.  The file is read twice in
 * near-parallel threads, hence the need to have two input chains.  
 * Output: context sorted uninterpolated probabilities and their interpolation 
 * weights.  
 */
void InitialProbabilities(const InitialProbabilitiesConfig &config, const std::vector<Discount> &discounts, Chains &primary, Chains &secondary);

} // namespace builder
} // namespace lm

#endif // LM_BUILDER_INITIAL_PROBABILITIES__

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
 * primary: the normal chain of n-grams.  Incoming is context sorted adjusted
 *   counts.  Outgoing has uninterpolated probabilities for use by Interpolate.
 * second_in: a second copy of the primary input.  Discard the output.  
 * gamma_out: Computed gamma values are output on these chains in suffix order.
 *   The values are bare floats and should be buffered for interpolation to
 *   use.  
 */
void InitialProbabilities(const InitialProbabilitiesConfig &config, const std::vector<Discount> &discounts, Chains &primary, Chains &second_in, Chains &gamma_out);

} // namespace builder
} // namespace lm

#endif // LM_BUILDER_INITIAL_PROBABILITIES__

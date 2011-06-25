#include "lm/quanize.hh"

#include "lm/lm_exception.hh"

#include <algorithm>
#include <numeric>

namespace lm {
namespace ngram {

/* Quantize into bins of equal size as described in
 * M. Federico and N. Bertoldi. 2006. How many bits are needed
 * to store probabilities for phrase-based translation? In Proc.
 * of the Workshop on Statistical Machine Translation, pages
 * 94–101, New York City, June. Association for Computa-
 * tional Linguistics.
 */

void Binner::Learn(float *centers, uint32_t bins) {
  std::sort(values_.begin(), values_.end());
  const float *start = values_.begin();
  for (uint32_t i = 0; i < bins; ++i, ++centers, start = finish) {
    const float *finish = values_.begin() + ((values_.size() * static_cast<uint64_t>(i + 1)) / bins);
    *centers = std::accumulate(start, finish, 0.0) / static_cast<float>(finish - start);
  }
}

SeparatelyQuantize::SeparatelyQuantize(const Config &config, uint8_t order) 
  : prob_(config.prob_bits),
    backoff_(config.backoff_bits),
    total_bits_(prob_.Bits() + backoff_.Bits()),
    total_mask_(1ULL << total_bits_ - 1),
    prob_longest_(prob_.Base(order)) {
  if (config.prob_bits > 25) UTIL_THROW(ConfigException, "For efficiency reasons, quantizing probability supports at most 25 bits.  Currently you have requested " << static_cast<unsigned>(config.prob_bits) << " bits.");
  if (config.backoff_bits > 25) UTIL_THROW(ConfigException, "For efficiency reasons, quantizing backoff supports at most 25 bits.  Currently you have requested " << static_cast<unsigned>(config.backoff_bits) << " bits.");
}

} // namespace ngram
} // namespace lm

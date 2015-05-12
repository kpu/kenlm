#ifndef KENLM_INTERPOLATE_BACKOFF_REUNIFICATION_
#define KENLM_INTERPOLATE_BACKOFF_REUNIFICATION_

#include "util/stream/stream.hh"
#include "util/stream/multi_stream.hh"

namespace lm {
namespace interpolate {

struct ReunifyConfig {
  util::stream::SortConfig sort_config;
  std::string file_base;
  std::size_t num_blocks;
  std::size_t max_ram;
};

/**
 * The third pass for the offline log-linear interpolation algorithm.
 *
 * @param prob_stream The stream that is currently reading probability
 *  values (ngram-id and probability) in *context* order
 * @param backoff_stream A stream of backoff values (just floats) in
 *  suffix-order
 */
void ReunifyBackoff(const ReunifyConfig &config,
                    util::stream::Chains &prob_chains,
                    util::stream::Chains &backoff_chains);
}
}
#endif

#ifndef KENLM_INTERPOLATE_INTERPOLATE_INFO_H
#define KENLM_INTERPOLATE_INTERPOLATE_INFO_H

#include "util/fixed_array.hh"

namespace lm {
namespace interpolate {

/**
 * Stores relevant info for interpolating several language models, for use
 * during the three-pass offline log-linear interpolation algorithm.
 */
struct InterpolateInfo {
  /**
   * Creates interpolation info storage for a specific number of models.
   * The actual data should be populated later, but the arrays will already
   * be allocated for you.
   */
  InterpolateInfo(std::size_t num_models) {
    lambdas.Init(num_models);
    orders.Init(num_models);
  }

  /**
   * @return the number of models being interpolated
   */
  std::size_t Models() const {
    return orders.size();
  }

  /**
   * The lambda (interpolation weight) for each model.
   */
  util::FixedArray<float> lambdas;

  /**
   * The maximum ngram order for each model.
   */
  util::FixedArray<uint8_t> orders;
};
}
}
#endif

#ifndef KENLM_INTERPOLATE_PIPELINE_H
#define KENLM_INTERPOLATE_PIPELINE_H

#include "lm/common/model_buffer.hh"
#include "util/fixed_array.hh"

#include <cstddef>
#include <string>

namespace lm { namespace interpolate {

struct Config {
  std::vector<float> lambdas;
  std::string temp_prefix;
  std::size_t buffer_size;
  std::size_t total_memory;
};

void Pipeline(util::FixedArray<ModelBuffer> &models, const Config &config);

}} // namespaces
#endif // KENLM_INTERPOLATE_PIPELINE_H

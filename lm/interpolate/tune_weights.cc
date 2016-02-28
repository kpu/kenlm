#include "lm/interpolate/tune_weights.hh"

#include "lm/interpolate/tune_derivatives.hh"
#include "lm/interpolate/tune_instances.hh"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas" // Older gcc doesn't have "-Wunused-local-typedefs" and complains.
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#include <Eigen/Dense>
#pragma GCC diagnostic pop
#include <boost/program_options.hpp>

#include <iostream>

namespace lm { namespace interpolate {
void TuneWeights(int tune_file, const std::vector<StringPiece> &model_names, const InstancesConfig &config, std::vector<float> &weights_out) {
  Instances instances(tune_file, model_names, config);
  Vector weights = Vector::Constant(model_names.size(), 1.0 / model_names.size());
  for (std::size_t iteration = 0; iteration < 10 /*TODO fancy stopping criteria */; ++iteration) {
    std::cerr << "Iteration " << iteration << ": weights = " << weights.transpose() << std::endl;
    Derivatives(instances, weights);
  }
  weights_out.assign(weights.data(), weights.data() + weights.size());
}
}} // namespaces

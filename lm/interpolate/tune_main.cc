#include "lm/interpolate/tune_derivatives.hh"
#include "lm/interpolate/tune_instance.hh"
#include "util/file.hh"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#include <Eigen/Dense>
#pragma GCC diagnostic pop
#include <boost/program_options.hpp>

#include <cmath>
#include <iostream>

namespace lm { namespace interpolate {
void TuneWeights(int tune_file, const std::vector<StringPiece> &model_names, Vector &weights) {
  util::FixedArray<Instance> instances;
  Matrix ln_unigrams;
  WordIndex bos = LoadInstances(tune_file, model_names, instances, ln_unigrams);
  ComputeDerivative derive(instances, ln_unigrams, bos);
  weights = Vector::Constant(model_names.size(), 1.0 / model_names.size());
  Vector gradient;
  Matrix hessian;
  for (std::size_t iteration = 0; iteration < 10 /*TODO fancy stopping criteria */; ++iteration) {
    std::cerr << "Iteration " << iteration << ": weights =";
    for (Vector::Index i = 0; i < weights.rows(); ++i) {
      std::cerr << ' ' << weights(i);
    }
    std::cerr << std::endl;
    std::cerr  << "Perplexity = " <<
      derive.Iteration(weights, gradient, hessian)
      << std::endl;
    // TODO: 1.0 step size was too big and it kept getting unstable.  More math.
    weights -= 0.7 * hessian.inverse() * gradient;
  }
}
}} // namespaces

int main(int argc, char *argv[]) {
  Eigen::initParallel();
  namespace po = boost::program_options;
  // TODO help
  po::options_description options("Tuning options");
  std::string tuning_file;
  std::vector<std::string> input_models;
  options.add_options()
    ("tuning,t", po::value<std::string>(&tuning_file)->required(), "File to tune on.  This should be a text file with one sentence per line.")
    ("model,m", po::value<std::vector<std::string> >(&input_models)->multitoken()->required(), "Models to interpolate");
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, options), vm);
  po::notify(vm);

  std::vector<StringPiece> model_names;
  for (std::vector<std::string>::const_iterator i = input_models.begin(); i != input_models.end(); ++i) {
    model_names.push_back(*i);
  }
  lm::interpolate::Vector weights;
  lm::interpolate::TuneWeights(util::OpenReadOrThrow(tuning_file.c_str()), model_names, weights);
  std::cout << weights.transpose() << std::endl;
}

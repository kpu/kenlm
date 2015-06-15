#include "lm/interpolate/tune_derivatives.hh"
#include "lm/interpolate/tune_instance.hh"
#include "util/file.hh"

#include <Eigen/Dense>
#include <boost/program_options.hpp>

#include <cmath>
#include <iostream>

namespace lm { namespace interpolate {
void TuneWeights(int tune_file, const std::vector<StringPiece> &model_names, Vector &weights) {
  util::FixedArray<Instance> instances;
  Matrix ln_unigrams;
  LoadInstances(tune_file, model_names, instances, ln_unigrams);
  ComputeDerivative derive(instances, ln_unigrams);
  weights = Vector::Zero(model_names.size());
  Vector gradient;
  Matrix hessian;
  for (std::size_t iteration = 0; iteration < 10 /*TODO*/; ++iteration) {
    derive.Iteration(weights, gradient, hessian);
    std::cerr << "gradient\n" << gradient << std::endl;
    std::cerr << "hessian\n" << hessian << std::endl;
    weights -= hessian.inverse() * gradient;
    std::cerr << "Weights\n" << weights << std::endl;
  }
  // Internally converted to ln, which is equivalent to upweighting.
  weights /= M_LN10;
}
}} // namespaces

int main(int argc, char *argv[]) {
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
  std::cout << weights << std::endl;
}

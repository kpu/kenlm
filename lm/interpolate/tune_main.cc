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
  WordIndex bos = LoadInstances(tune_file, model_names, instances, ln_unigrams);
  ComputeDerivative derive(instances, ln_unigrams, bos);
  weights = Vector::Constant(model_names.size(), 1.0 / model_names.size());
  Vector gradient;
  Matrix hessian;
  for (std::size_t iteration = 0; iteration < 5 /*TODO*/; ++iteration) {
    std::cerr << "Weights " << weights << std::endl;
    std::cerr << "Perplexity = " <<
      derive.Iteration(weights, gradient, hessian)
      << std::endl;
    std::cerr << "Gradient = \n" << gradient << "\n hessian = \n" << hessian << "\n";
    weights -= hessian.inverse() * gradient;
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

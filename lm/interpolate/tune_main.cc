#include "lm/common/size_option.hh"
#include "lm/interpolate/tune_derivatives.hh"
#include "lm/interpolate/tune_instances.hh"
#include "util/file.hh"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas" // Older gcc doesn't have "-Wunused-local-typedefs" and complains.
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#include <Eigen/Dense>
#pragma GCC diagnostic pop
#include <boost/program_options.hpp>

#include <cmath>
#include <iostream>

namespace lm { namespace interpolate {
void TuneWeights(int tune_file, const std::vector<StringPiece> &model_names, const InstancesConfig &config, Vector &weights) {
  Instances instances(tune_file, model_names, config);
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
      Derivatives(instances, weights, gradient, hessian)
      << std::endl;
    // TODO: 1.0 step size was too big and it kept getting unstable.  More math.
    weights -= 0.7 * hessian.inverse() * gradient;
  }
}
}} // namespaces

int main(int argc, char *argv[]) {
  Eigen::initParallel();
  namespace po = boost::program_options;
  lm::interpolate::InstancesConfig config;
  // TODO help
  po::options_description options("Tuning options");
  std::string tuning_file;
  std::vector<std::string> input_models;
  options.add_options()
    ("help,h", po::bool_switch(), "Show this help message")
    ("tuning,t", po::value<std::string>(&tuning_file)->required(), "File to tune on.  This should be a text file with one sentence per line.")
    ("model,m", po::value<std::vector<std::string> >(&input_models)->multitoken()->required(), "Models to interpolate")
    ("temp_prefix,T", po::value<std::string>(&config.sort.temp_prefix)->default_value("/tmp/lm"), "Temporary file prefix")
    ("memory,S", lm::SizeOption(config.sort.total_memory, "1G"), "Memory to use in addition to unigram and backoff storage")
    ("sort_block", lm::SizeOption(config.sort.buffer_size, "64M"), "Size of IO operations for sort (determines arity)");
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, options), vm);
  if (argc == 1 || vm["help"].as<bool>()) {
    std::cerr <<
      "Tunes interpolation weights.\n\n"
      << options << std::endl;
    return 1;
  }

  po::notify(vm);

  config.lazy_memory = config.sort.total_memory;
  config.extension_write_chain_mem = config.sort.total_memory;
  config.model_read_chain_mem = config.sort.buffer_size;

  std::vector<StringPiece> model_names;
  for (std::vector<std::string>::const_iterator i = input_models.begin(); i != input_models.end(); ++i) {
    model_names.push_back(*i);
  }
  lm::interpolate::Vector weights;
  lm::interpolate::TuneWeights(util::OpenReadOrThrow(tuning_file.c_str()), model_names, config, weights);
  std::cout << weights.transpose() << std::endl;
}

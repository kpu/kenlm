#include "lm/common/model_buffer.hh"
#include "lm/common/size_option.hh"
#include "lm/interpolate/pipeline.hh"
#include "util/fixed_array.hh"
#include "util/usage.hh"

#include <boost/program_options.hpp>

#include <iostream>
#include <vector>

int main(int argc, char *argv[]) {
  lm::interpolate::Config config;
  std::vector<std::string> input_models;
  namespace po = boost::program_options;
  po::options_description options("Log-linear interpolation options");
  options.add_options()
    ("lambda,w", po::value<std::vector<float> >(&config.lambdas)->multitoken()->required(), "Interpolation weights")
    ("model,m", po::value<std::vector<std::string> >(&input_models)->multitoken()->required(), "Models to interpolate")
    ("temp_prefix,T", po::value<std::string>(&config.sort.temp_prefix)->default_value("/tmp/lm"), "Temporary file prefix")
    ("memory,S", lm::SizeOption(config.sort.total_memory, util::GuessPhysicalMemory() ? "50%" : "1G"), "Sorting memory")
    ("sort_block", lm::SizeOption(config.sort.buffer_size, "64M"), "Block size");
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, options), vm);
  po::notify(vm);

  if (config.lambdas.size() != input_models.size()) {
    std::cerr << "Number of models " << input_models.size() << " should match the number of weights" << config.lambdas.size() << "." << std::endl;
    return 1;
  }

  util::FixedArray<lm::ModelBuffer> models(input_models.size());
  for (std::size_t i = 0; i < input_models.size(); ++i) {
    models.push_back(input_models[i]);
  }
  lm::interpolate::Pipeline(models, config, 1);
}

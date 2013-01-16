#include "lm/builder/pipeline.hh"
#include "util/file_piece.hh"
#include "util/usage.hh"

#include <iostream>

#include <boost/program_options.hpp>

int main(int argc, char *argv[]) {
  try {
    namespace po = boost::program_options;
    po::options_description options("Language model building options");
    lm::builder::PipelineConfig pipeline;
    options.add_options()
      ("order,o", po::value<std::size_t>(&pipeline.order)->required(), "Order of the model")
      ("temp_prefix,t", po::value<std::string>(&pipeline.sort.temp_prefix)->default_value("/tmp/lm"), "Temporary file prefix")
      ("vocab_file,v", po::value<std::string>(&pipeline.vocab_file)->default_value(""), "Location to write vocabulary file")
      ("vocab_memory", po::value<std::size_t>(&pipeline.assume_vocab_hash_size)->default_value(1 << 24), "Assume that the vocabulary hash table will use this much memory for purposes of calculating total memory in the count step")
      ("chain_memory", po::value<std::size_t>(&pipeline.chain.total_memory)->default_value(1 << 27), "Memory for each chain")
      ("block_count", po::value<std::size_t>(&pipeline.chain.block_count)->default_value(2), "Block count (per order)")
      ("minimum_block", po::value<std::size_t>(&pipeline.minimum_block)->default_value(1 << 13), "Minimum block size to allow")
      ("sort_memory,S", po::value<std::size_t>(&pipeline.sort.total_memory)->default_value(1 << 30), "Sorting memory")
      ("sort_block", po::value<std::size_t>(&pipeline.sort.buffer_size)->default_value(1 << 26), "Size of IO operations for sort (determines arity)")
      ("interpolate_unigrams", po::bool_switch(&pipeline.initial_probs.interpolate_unigrams), "Interpolate the unigrams (default: emulate SRILM by not interpolating)")
      ("verbose_header,V", po::bool_switch(&pipeline.verbose_header), "Add a verbose header to the ARPA file that includes information such as token count, smoothing type, etc.");
    if (argc == 1) {
      std::cerr << options << std::endl;
      return 1;
    }
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, options), vm);
    po::notify(vm);
    pipeline.chain.entry_size = 0;

    lm::builder::InitialProbabilitiesConfig &initial = pipeline.initial_probs;
    // TODO: evaluate options for these.  
    initial.adder_in.total_memory = 32768;
    initial.adder_in.block_count = 2;
    initial.adder_out.total_memory = 32768;
    initial.adder_out.block_count = 2;
    pipeline.read_backoffs = initial.adder_out;

    // Read from stdin
    lm::builder::Pipeline(pipeline, 0, std::cout);
    util::PrintUsage(std::cerr);
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
}

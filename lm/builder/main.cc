#include "lm/builder/pipeline.hh"
#include "util/file_piece.hh"
#include "util/usage.hh"

#include <iostream>

#include <boost/program_options.hpp>

int main(int argc, char *argv[]) {
  //try {
    namespace po = boost::program_options;
    po::options_description options("Language model building options");
    lm::builder::PipelineConfig pipeline;
    options.add_options()
      ("order,o", po::value<std::size_t>(&pipeline.order)->required(), "Order of the model")
      ("temp_prefix,t", po::value<std::string>(&pipeline.sort.temp_prefix)->default_value("/tmp/lm"), "Temporary file prefix")
      ("vocab_file,v", po::value<std::string>(&pipeline.vocab_file)->default_value(""), "Location to write vocabulary file")
      ("chain_memory", po::value<std::size_t>(&pipeline.chain.total_memory)->default_value(1 << 26), "Memory for each chain")
      ("block_count", po::value<std::size_t>(&pipeline.chain.block_count)->default_value(2), "Block count (per order)")
      ("sort_arity,a", po::value<std::size_t>(&pipeline.sort.arity)->default_value(4), "Arity to use for sorting")
      ("sort_buffer", po::value<std::size_t>(&pipeline.sort.total_read_buffer)->default_value(1 << 26), "Sort read buffer size")
      ("sort_lazy_arity", po::value<std::size_t>(&pipeline.sort.lazy_arity)->default_value(2), "Lazy sorting arity (this * order readers active)")
      ("sort_lazy_buffer", po::value<std::size_t>(&pipeline.sort.lazy_total_read_buffer)->default_value(1 << 25), "Lazy sorting read buffer size")
      ("interpolate_unigrams", "Interpolate the unigrams (default: emulate SRILM by not interpolating)")
      ("verbose_header,V", "Add a verbose header to the ARPA file that includes information such as token count, smoothing type, etc.");
    if (argc == 1) {
      std::cerr << options << std::endl;
      return 1;
    }
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, options), vm);
    po::notify(vm);
    pipeline.chain.entry_size = 0;
    pipeline.sort.chain = pipeline.chain;
    pipeline.verbose_header = !!vm.count("verbose_header");

    lm::builder::InitialProbabilitiesConfig &initial = pipeline.initial_probs;
    initial.interpolate_unigrams = !!vm.count("interpolate_unigrams");
    // TODO: evaluate options for these.  
    initial.adder_in.total_memory = 32768;
    initial.adder_in.block_count = 2;
    initial.adder_out.total_memory = 32768;
    initial.adder_out.block_count = 2;
    pipeline.read_backoffs = initial.adder_out;

    util::FilePiece in(0, "stdin", &std::cerr);
    lm::builder::Pipeline(pipeline, in, std::cout);
    util::PrintUsage(std::cerr);
/*  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }*/
}

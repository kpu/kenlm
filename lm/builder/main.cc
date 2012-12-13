#include "lm/builder/pipeline.hh"
#include "util/file_piece.hh"
#include "util/usage.hh"

#include <iostream>

#include <boost/program_options.hpp>

int main(int argc, char *argv[]) {
  namespace po = boost::program_options;
  po::options_description options("Language model building options");
  lm::builder::PipelineConfig pipeline;
  options.add_options()
    ("order,o", po::value<std::size_t>(&pipeline.order)->required(), "Order of the model")
    ("temp_prefix,t", po::value<std::string>(&pipeline.sort.temp_prefix)->default_value("/tmp/lm"), "Temporary file prefix")
    ("vocab_file,v", po::value<std::string>(&pipeline.vocab_file)->default_value(""), "Location to write vocabulary file")
    ("block_size,b", po::value<std::size_t>(&pipeline.chain.block_size)->default_value(1 << 26), "Block size")
    ("block_count", po::value<std::size_t>(&pipeline.chain.block_count)->default_value(2), "Block count (per order)")
    ("queue_length", po::value<std::size_t>(&pipeline.chain.queue_length)->default_value(2), "Message queue length")
    ("sort_arity,a", po::value<std::size_t>(&pipeline.sort.arity)->default_value(4), "Arity to use for sorting")
    ("sort_buffer", po::value<std::size_t>(&pipeline.sort.total_read_buffer)->default_value(1 << 26), "Sort read buffer size")
    ("sort_lazy_arity", po::value<std::size_t>(&pipeline.sort.lazy_arity)->default_value(2), "Lazy sorting arity (this * order readers active)")
    ("sort_lazy_buffer", po::value<std::size_t>(&pipeline.sort.lazy_total_read_buffer)->default_value(1 << 25), "Lazy sorting read buffer size");
  if (argc == 1) {
    std::cerr << options << std::endl;
    return 1;
  }
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, options), vm);
  po::notify(vm);
  pipeline.chain.entry_size = 0;
  pipeline.sort.chain = pipeline.chain;

  util::FilePiece in(0, "stdin");
  lm::builder::Pipeline(pipeline, in, std::cout);
  util::PrintUsage(std::cerr);
}

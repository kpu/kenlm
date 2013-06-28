#include "lm/builder/pipeline.hh"
#include "util/file.hh"
#include "util/file_piece.hh"
#include "util/usage.hh"

#include <iostream>

#include <boost/program_options.hpp>
#include <boost/version.hpp>

namespace {
class SizeNotify {
  public:
    SizeNotify(std::size_t &out) : behind_(out) {}

    void operator()(const std::string &from) {
      behind_ = util::ParseSize(from);
    }

  private:
    std::size_t &behind_;
};

boost::program_options::typed_value<std::string> *SizeOption(std::size_t &to, const char *default_value) {
  return boost::program_options::value<std::string>()->notifier(SizeNotify(to))->default_value(default_value);
}

} // namespace

int main(int argc, char *argv[]) {
  try {
    namespace po = boost::program_options;
    po::options_description options("Language model building options");
    lm::builder::PipelineConfig pipeline;

    std::string text, arpa;

    options.add_options()
      ("order,o", po::value<std::size_t>(&pipeline.order)
#if BOOST_VERSION >= 104200
         ->required()
#endif
         , "Order of the model")
      ("interpolate_unigrams", po::bool_switch(&pipeline.initial_probs.interpolate_unigrams), "Interpolate the unigrams (default: emulate SRILM by not interpolating)")
      ("temp_prefix,T", po::value<std::string>(&pipeline.sort.temp_prefix)->default_value("/tmp/lm"), "Temporary file prefix")
      ("memory,S", SizeOption(pipeline.sort.total_memory, util::GuessPhysicalMemory() ? "80%" : "1G"), "Sorting memory")
      ("minimum_block", SizeOption(pipeline.minimum_block, "8K"), "Minimum block size to allow")
      ("sort_block", SizeOption(pipeline.sort.buffer_size, "64M"), "Size of IO operations for sort (determines arity)")
      ("vocab_estimate", po::value<lm::WordIndex>(&pipeline.vocab_estimate)->default_value(1000000), "Assume this vocabulary size for purposes of calculating memory in step 1 (corpus count) and pre-sizing the hash table")
      ("block_count", po::value<std::size_t>(&pipeline.block_count)->default_value(2), "Block count (per order)")
      ("vocab_file", po::value<std::string>(&pipeline.vocab_file)->default_value(""), "Location to write vocabulary file")
      ("verbose_header", po::bool_switch(&pipeline.verbose_header), "Add a verbose header to the ARPA file that includes information such as token count, smoothing type, etc.")
      ("text", po::value<std::string>(&text), "Read text from a file instead of stdin")
      ("arpa", po::value<std::string>(&arpa), "Write ARPA to a file instead of stdout");
    if (argc == 1) {
      std::cerr << 
        "Builds unpruned language models with modified Kneser-Ney smoothing.\n\n"
        "Please cite:\n"
        "@inproceedings{Heafield-estimate,\n"
        "  author = {Kenneth Heafield and Ivan Pouzyrevsky and Jonathan H. Clark and Philipp Koehn},\n"
        "  title = {Scalable Modified {Kneser-Ney} Language Model Estimation},\n"
        "  year = {2013},\n"
        "  month = {8},\n"
        "  booktitle = {Proceedings of the 51st Annual Meeting of the Association for Computational Linguistics},\n"
        "  address = {Sofia, Bulgaria},\n"
        "  url = {http://kheafield.com/professional/edinburgh/estimate\\_paper.pdf},\n"
        "}\n\n"
        "Provide the corpus on stdin.  The ARPA file will be written to stdout.  Order of\n"
        "the model (-o) is the only mandatory option.  As this is an on-disk program,\n"
        "setting the temporary file location (-T) and sorting memory (-S) is recommended.\n\n"
        "Memory sizes are specified like GNU sort: a number followed by a unit character.\n"
        "Valid units are \% for percentage of memory (supported platforms only) and (in\n"
        "increasing powers of 1024): b, K, M, G, T, P, E, Z, Y.  Default is K (*1024).\n\n";
      std::cerr << options << std::endl;
      return 1;
    }
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, options), vm);
    po::notify(vm);

    // required() appeared in Boost 1.42.0.
#if BOOST_VERSION < 104200
    if (!vm.count("order")) {
      std::cerr << "the option '--order' is required but missing" << std::endl;
      return 1;
    }
#endif

    util::NormalizeTempPrefix(pipeline.sort.temp_prefix);

    lm::builder::InitialProbabilitiesConfig &initial = pipeline.initial_probs;
    // TODO: evaluate options for these.  
    initial.adder_in.total_memory = 32768;
    initial.adder_in.block_count = 2;
    initial.adder_out.total_memory = 32768;
    initial.adder_out.block_count = 2;
    pipeline.read_backoffs = initial.adder_out;

    util::scoped_fd in(0), out(1);
    if (vm.count("text")) {
      in.reset(util::OpenReadOrThrow(text.c_str()));
    }
    if (vm.count("arpa")) {
      out.reset(util::CreateOrThrow(arpa.c_str()));
    }

    // Read from stdin
    try {
      lm::builder::Pipeline(pipeline, in.release(), out.release());
    } catch (const util::MallocException &e) {
      std::cerr << e.what() << std::endl;
      std::cerr << "Try rerunning with a more conservative -S setting than " << vm["memory"].as<std::string>() << std::endl;
      return 1;
    }
    util::PrintUsage(std::cerr);
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
}

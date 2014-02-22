#include "lm/builder/pipeline.hh"
#include "lm/lm_exception.hh"
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

    std::string arpa;
    std::vector<std::string> inputs;
    lm::WordIndex vocab_size;

    options.add_options()
      ("help,h", po::bool_switch(), "Show this help message")
      ("order,o", po::value<std::size_t>(&pipeline.order)
#if BOOST_VERSION >= 104200
         ->required()
#endif
         , "Order of the model")
      ("interpolate_unigrams", po::bool_switch(&pipeline.initial_probs.interpolate_unigrams), "Interpolate the unigrams (default: emulate SRILM by not interpolating)")
      ("skip_symbols", po::bool_switch(), "Treat <s>, </s>, and <unk> as whitespace instead of throwing an exception")
      ("temp_prefix,T", po::value<std::string>(&pipeline.sort.temp_prefix)->default_value("/tmp/lm"), "Temporary file prefix")
      ("memory,S", SizeOption(pipeline.sort.total_memory, util::GuessPhysicalMemory() ? "80%" : "1G"), "Sorting memory")
      ("minimum_block", SizeOption(pipeline.minimum_block, "8K"), "Minimum block size to allow")
      ("sort_block", SizeOption(pipeline.sort.buffer_size, "64M"), "Size of IO operations for sort (determines arity)")
      ("block_count", po::value<std::size_t>(&pipeline.block_count)->default_value(2), "Block count (per order)")
      ("vocab_estimate", po::value<lm::WordIndex>(&pipeline.vocab_estimate)->default_value(1000000), "Assume this vocabulary size for purposes of calculating memory in step 1 (corpus count) and pre-sizing the hash table")
      ("vocab_file", po::value<std::string>(&pipeline.vocab_file)->required(), "Location to write a file containing the unique vocabulary strings delimited by null bytes")
      ("vocab_pad", po::value<std::size_t>(&pipeline.vocab_size_for_unk)->default_value(0), "If the vocabulary is smaller than this value, pad with <unk> to reach this size. Requires --interpolate_unigrams")
      ("verbose_header", po::bool_switch(&pipeline.verbose_header), "Add a verbose header to the ARPA file that includes information such as token count, smoothing type, etc.")
      ("inputs", po::value<std::vector<std::string> >(&inputs)->required()->composing(), "Files to merge")
      ("arpa", po::value<std::string>(&arpa), "Write ARPA to a file instead of stdout")
      ("vocab_size", po::value<lm::WordIndex>(&vocab_size)->required(), "Vocabulary size");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, options), vm);

    if (argc == 1 || vm["help"].as<bool>()) {
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
        "increasing powers of 1024): b, K, M, G, T, P, E, Z, Y.  Default is K (*1024).\n";
      uint64_t mem = util::GuessPhysicalMemory();
      if (mem) {
        std::cerr << "This machine has " << mem << " bytes of memory.\n\n";
      } else {
        std::cerr << "Unable to determine the amount of memory on this machine.\n\n";
      } 
      std::cerr << options << std::endl;
      return 1;
    }

    po::notify(vm);

    // required() appeared in Boost 1.42.0.
#if BOOST_VERSION < 104200
    if (!vm.count("order")) {
      std::cerr << "the option '--order' is required but missing" << std::endl;
      return 1;
    }
#endif

    if (pipeline.vocab_size_for_unk && !pipeline.initial_probs.interpolate_unigrams) {
      std::cerr << "--vocab_pad requires --interpolate_unigrams" << std::endl;
      return 1;
    }

    if (vm["skip_symbols"].as<bool>()) {
      pipeline.disallowed_symbol_action = lm::COMPLAIN;
    } else {
      pipeline.disallowed_symbol_action = lm::THROW_UP;
    }

    util::NormalizeTempPrefix(pipeline.sort.temp_prefix);

    lm::builder::InitialProbabilitiesConfig &initial = pipeline.initial_probs;
    // TODO: evaluate options for these.  
    initial.adder_in.total_memory = 32768;
    initial.adder_in.block_count = 2;
    initial.adder_out.total_memory = 32768;
    initial.adder_out.block_count = 2;
    pipeline.read_backoffs = initial.adder_out;

    try {
      // HACK!
      lm::builder::Pipeline(pipeline, inputs, vocab_size, 1);
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

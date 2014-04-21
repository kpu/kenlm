#include "lm/builder/pipeline.hh"
#include "lm/lm_exception.hh"
#include "util/file.hh"
#include "util/file_piece.hh"
#include "util/usage.hh"

#include <iostream>

#include <boost/program_options.hpp>
#include <boost/version.hpp>
#include <vector>

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

// Parse and validate pruning thresholds then return vector of threshold counts
// for each n-grams order.
std::vector<uint64_t> ParsePruning(const std::vector<std::string> &param, std::size_t order) {
  // convert to vector of integers
  std::vector<uint64_t> prune_thresholds;
  prune_thresholds.reserve(order);
  std::cerr << "Pruning ";
  for (std::vector<std::string>::const_iterator it(param.begin()); it != param.end(); ++it) {
    try {
      prune_thresholds.push_back(boost::lexical_cast<uint64_t>(*it));
    } catch(const boost::bad_lexical_cast &) {
      UTIL_THROW(util::Exception, "Bad pruning threshold " << *it);
    }
  }

  // Fill with zeros by default.
  if (prune_thresholds.empty()) {
    prune_thresholds.resize(order, 0);
    return prune_thresholds;
  }

  // validate pruning threshold if specified
  // throw if each n-gram order has not  threshold specified
  UTIL_THROW_IF(prune_thresholds.size() > order, util::Exception, "You specified pruning thresholds for orders 1 through " << prune_thresholds.size() << " but the model only has order " << order);
  // threshold for unigram can only be 0 (no pruning)
  UTIL_THROW_IF(prune_thresholds[0] != 0, util::Exception, "Unigram pruning is not implemented, so the first pruning threshold must be 0.");

  // check if threshold are not in decreasing order
  uint64_t lower_threshold = 0;
  for (std::vector<uint64_t>::iterator it = prune_thresholds.begin(); it != prune_thresholds.end(); ++it) {
    UTIL_THROW_IF(lower_threshold > *it, util::Exception, "Pruning thresholds should be in non-decreasing order.  Otherwise substrings would be removed, which is bad for query-time data structures.");
    lower_threshold = *it;
  }

  // Pad to all orders using the last value.
  prune_thresholds.resize(order, prune_thresholds.back());
  return prune_thresholds;
}


} // namespace

int main(int argc, char *argv[]) {
  try {
    namespace po = boost::program_options;
    po::options_description options("Language model building options");
    lm::builder::PipelineConfig pipeline;

    std::string text, arpa;
    std::vector<std::string> pruning;


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
      ("vocab_file", po::value<std::string>(&pipeline.vocab_file)->default_value(""), "Location to write a file containing the unique vocabulary strings delimited by null bytes")
      ("vocab_pad", po::value<uint64_t>(&pipeline.vocab_size_for_unk)->default_value(0), "If the vocabulary is smaller than this value, pad with <unk> to reach this size. Requires --interpolate_unigrams")
      ("verbose_header", po::bool_switch(&pipeline.verbose_header), "Add a verbose header to the ARPA file that includes information such as token count, smoothing type, etc.")
      ("text", po::value<std::string>(&text), "Read text from a file instead of stdin")
      ("arpa", po::value<std::string>(&arpa), "Write ARPA to a file instead of stdout")
      ("prune", po::value<std::vector<std::string> >(&pruning)->multitoken(), "Prune n-grams with count less than or equal to the given threshold.  Specify one value for each order i.e. 0 0 1 to prune singleton trigrams and above.  The sequence of values must be non-decreasing and the last value applies to any remaining orders.  Unigram pruning is not implemented, so the first value must be zero.  Default is to not prune, which is equivalent to --prune 0.");
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

    // parse pruning thresholds.  These depend on order, so it is not done as a notifier.
    pipeline.prune_thresholds = ParsePruning(pruning, pipeline.order);
    
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

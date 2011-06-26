#include "lm/model.hh"
#include "util/file_piece.hh"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <iomanip>

#include <math.h>
#include <stdlib.h>
#include <unistd.h>

namespace lm {
namespace ngram {
namespace {

void Usage(const char *name) {
  std::cerr << "Usage: " << name << " [-u log10_unknown_probability] [-s] [-n] [-p probing_multiplier] [-t trie_temporary] [-m trie_building_megabytes] [-q bits] [-b bits] [type] input.arpa output.mmap\n\n"
"-u sets the default log10 probability for <unk> if the ARPA file does not have\n"
"one.\n"
"-s allows models to be built even if they do not have <s> and </s>.\n"
"-i allows buggy models from IRSTLM by mapping positive log probability to 0.\n"
"type is either probing or trie:\n\n"
"probing uses a probing hash table.  It is the fastest but uses the most memory.\n"
"-p sets the space multiplier and must be >1.0.  The default is 1.5.\n\n"
"trie is a straightforward trie with bit-level packing.  It uses the least\n"
"memory and is still faster than SRI or IRST.  Building the trie format uses an\n"
"on-disk sort to save memory.\n"
"-t is the temporary directory prefix.  Default is the output file name.\n"
"-m limits memory use for sorting.  Measured in MB.  Default is 1024MB.\n"
"-q turns quantization on and sets the number of bits (e.g. -q 8).\n"
"-b sets backoff quantization bits.  Requires -q and defaults to that value.\n\n"
"See http://kheafield.com/code/kenlm/benchmark/ for data structure benchmarks.\n"
"Passing only an input file will print memory usage of each data structure.\n"
"If the ARPA file does not have <unk>, -u sets <unk>'s probability; default 0.0.\n";
  exit(1);
}

// I could really use boost::lexical_cast right about now.  
float ParseFloat(const char *from) {
  char *end;
  float ret = strtod(from, &end);
  if (*end) throw util::ParseNumberException(from);
  return ret;
}
unsigned long int ParseUInt(const char *from) {
  char *end;
  unsigned long int ret = strtoul(from, &end, 10);
  if (*end) throw util::ParseNumberException(from);
  return ret;
}

uint8_t ParseBitCount(const char *from) {
  unsigned long val = ParseUInt(from);
  if (val > 25) {
    util::ParseNumberException e(from);
    e << " bit counts are limited to 256.";
  }
  return val;
}

void ShowSizes(const char *file, const lm::ngram::Config &config) {
  std::vector<uint64_t> counts;
  util::FilePiece f(file);
  lm::ReadARPACounts(f, counts);
  std::size_t sizes[3];
  sizes[0] = ProbingModel::Size(counts, config);
  sizes[1] = TrieModel::Size(counts, config);
  sizes[2] = QuantTrieModel::Size(counts, config);
  std::size_t max_length = *std::max_element(sizes, sizes + 3);
  std::size_t min_length = *std::max_element(sizes, sizes + 3);
  std::size_t divide;
  char prefix;
  if (min_length < (1 << 10) * 10) {
    prefix = ' ';
    divide = 1;
  } else if (min_length < (1 << 20) * 10) {
    prefix = 'k';
    divide = 1 << 10;
  } else if (min_length < (1ULL << 30) * 10) {
    prefix = 'M';
    divide = 1 << 20;
  } else {
    prefix = 'G';
    divide = 1 << 30;
  }
  long int length = std::max<long int>(2, lrint(ceil(log10(max_length / divide))));
  std::cout << "Memory estimate:\ntype    ";
  // right align bytes.  
  for (long int i = 0; i < length - 2; ++i) std::cout << ' ';
  std::cout << prefix << "B\n"
    "probing " << std::setw(length) << (sizes[0] / divide) << " assuming -p " << config.probing_multiplier << "\n"
    "trie    " << std::setw(length) << (sizes[1] / divide) << " without quantization\n"
    "trie    " << std::setw(length) << (sizes[2] / divide) << " assuming -q " << (unsigned)config.prob_bits << " -b " << (unsigned)config.backoff_bits << " quantization \n";
}

void ProbingQuantizationUnsupported() {
  std::cerr << "Quantization is only implemented in the trie data structure." << std::endl;
  exit(1);
}

} // namespace ngram
} // namespace lm
} // namespace

int main(int argc, char *argv[]) {
  using namespace lm::ngram;

  bool quantize = false, set_backoff_bits = false;
  try {
    lm::ngram::Config config;
    int opt;
    while ((opt = getopt(argc, argv, "siu:p:t:m:q:b:")) != -1) {
      switch(opt) {
        case 'q':
          config.prob_bits = ParseBitCount(optarg);
          if (!set_backoff_bits) config.backoff_bits = config.prob_bits;
          quantize = true;
          break;
        case 'b':
          config.backoff_bits = ParseBitCount(optarg);
          set_backoff_bits = true;
          break;
        case 'u':
          config.unknown_missing_logprob = ParseFloat(optarg);
          break;
        case 'p':
          config.probing_multiplier = ParseFloat(optarg);
          break;
        case 't':
          config.temporary_directory_prefix = optarg;
          break;
        case 'm':
          config.building_memory = ParseUInt(optarg) * 1048576;
          break;
        case 's':
          config.sentence_marker_missing = lm::SILENT;
          break;
        case 'i':
          config.positive_log_probability = lm::SILENT;
          break;
        default:
          Usage(argv[0]);
      }
    }
    if (!quantize && set_backoff_bits) {
      std::cerr << "You specified backoff quantization (-b) but not probability quantization (-q)" << std::endl;
      abort();
    }
    if (optind + 1 == argc) {
      ShowSizes(argv[optind], config);
    } else if (optind + 2 == argc) {
      config.write_mmap = argv[optind + 1];
      if (quantize || set_backoff_bits) ProbingQuantizationUnsupported();
      ProbingModel(argv[optind], config);
    } else if (optind + 3 == argc) {
      const char *model_type = argv[optind];
      const char *from_file = argv[optind + 1];
      config.write_mmap = argv[optind + 2];
      if (!strcmp(model_type, "probing")) {
        if (quantize || set_backoff_bits) ProbingQuantizationUnsupported();
        ProbingModel(from_file, config);
      } else if (!strcmp(model_type, "trie")) {
        if (quantize) {
          QuantTrieModel(from_file, config);
        } else {
          TrieModel(from_file, config);
        }
      } else {
        Usage(argv[0]);
      }
    } else {
      Usage(argv[0]);
    }
  }
  catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
    abort();
  }
  return 0;
}

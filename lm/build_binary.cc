#include "lm/model.hh"
#include "util/file_piece.hh"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <iomanip>

#include <math.h>
#include <stdlib.h>

#ifdef WIN32
#include "util/getopt.hh"
#endif

namespace lm {
namespace ngram {
namespace {

void Usage(const char *name) {
  std::cerr << "Usage: " << name << " [-u log10_unknown_probability] [-s] [-i] [-w mmap|after] [-p probing_multiplier] [-t trie_temporary] [-m trie_building_megabytes] [-q bits] [-b bits] [-a bits] [type] input.arpa [output.mmap]\n\n"
"-u sets the log10 probability for <unk> if the ARPA file does not have one.\n"
"   Default is -100.  The ARPA file will always take precedence.\n"
"-s allows models to be built even if they do not have <s> and </s>.\n"
"-i allows buggy models from IRSTLM by mapping positive log probability to 0.\n"
"-w mmap|after determines how writing is done.\n"
"   mmap maps the binary file and writes to it.  Default for trie.\n"
"   after allocates anonymous memory, builds, and writes.  Default for probing.\n\n"
"type is either probing or trie.  Default is probing.\n\n"
"probing uses a probing hash table.  It is the fastest but uses the most memory.\n"
"-p sets the space multiplier and must be >1.0.  The default is 1.5.\n\n"
"trie is a straightforward trie with bit-level packing.  It uses the least\n"
"memory and is still faster than SRI or IRST.  Building the trie format uses an\n"
"on-disk sort to save memory.\n"
"-t is the temporary directory prefix.  Default is the output file name.\n"
"-m limits memory use for sorting.  Measured in MB.  Default is 1024MB.\n"
"-q turns quantization on and sets the number of bits (e.g. -q 8).\n"
"-b sets backoff quantization bits.  Requires -q and defaults to that value.\n"
"-a compresses pointers using an array of offsets.  The parameter is the\n"
"   maximum number of bits encoded by the array.  Memory is minimized subject\n"
"   to the maximum, so pick 255 to minimize memory.\n\n"
"Get a memory estimate by passing an ARPA file without an output file name.\n";
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
    e << " bit counts are limited to 25.";
  }
  return val;
}

void ShowSizes(const char *file, const lm::ngram::Config &config) {
  std::vector<uint64_t> counts;
  util::FilePiece f(file);
  lm::ReadARPACounts(f, counts);
  std::size_t sizes[5];
  sizes[0] = ProbingModel::Size(counts, config);
  sizes[1] = TrieModel::Size(counts, config);
  sizes[2] = QuantTrieModel::Size(counts, config);
  sizes[3] = ArrayTrieModel::Size(counts, config);
  sizes[4] = QuantArrayTrieModel::Size(counts, config);
  std::size_t max_length = *std::max_element(sizes, sizes + sizeof(sizes) / sizeof(size_t));
  std::size_t min_length = *std::min_element(sizes, sizes + sizeof(sizes) / sizeof(size_t));
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
  long int length = std::max<long int>(2, static_cast<long int>(ceil(log10((double) max_length / divide))));
  std::cout << "Memory estimate:\ntype    ";
  // right align bytes.  
  for (long int i = 0; i < length - 2; ++i) std::cout << ' ';
  std::cout << prefix << "B\n"
    "probing " << std::setw(length) << (sizes[0] / divide) << " assuming -p " << config.probing_multiplier << "\n"
    "trie    " << std::setw(length) << (sizes[1] / divide) << " without quantization\n"
    "trie    " << std::setw(length) << (sizes[2] / divide) << " assuming -q " << (unsigned)config.prob_bits << " -b " << (unsigned)config.backoff_bits << " quantization \n"
    "trie    " << std::setw(length) << (sizes[3] / divide) << " assuming -a " << (unsigned)config.pointer_bhiksha_bits << " array pointer compression\n"
    "trie    " << std::setw(length) << (sizes[4] / divide) << " assuming -a " << (unsigned)config.pointer_bhiksha_bits << " -q " << (unsigned)config.prob_bits << " -b " << (unsigned)config.backoff_bits<< " array pointer compression and quantization\n";
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

  try {
    bool quantize = false, set_backoff_bits = false, bhiksha = false, set_write_method = false;
    lm::ngram::Config config;
    int opt;
    while ((opt = getopt(argc, argv, "q:b:a:u:p:t:m:w:si")) != -1) {
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
        case 'a':
          config.pointer_bhiksha_bits = ParseBitCount(optarg);
          bhiksha = true;
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
        case 'w':
          set_write_method = true;
          if (!strcmp(optarg, "mmap")) {
            config.write_method = Config::WRITE_MMAP;
          } else if (!strcmp(optarg, "after")) {
            config.write_method = Config::WRITE_AFTER;
          } else {
            Usage(argv[0]);
          }
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
        if (!set_write_method) config.write_method = Config::WRITE_AFTER;
        if (quantize || set_backoff_bits) ProbingQuantizationUnsupported();
        ProbingModel(from_file, config);
      } else if (!strcmp(model_type, "trie")) {
        if (!set_write_method) config.write_method = Config::WRITE_MMAP;
        if (quantize) {
          if (bhiksha) {
            QuantArrayTrieModel(from_file, config);
          } else {
            QuantTrieModel(from_file, config);
          }
        } else {
          if (bhiksha) {
            ArrayTrieModel(from_file, config);
          } else {
            TrieModel(from_file, config);
          }
        }
      } else {
        Usage(argv[0]);
      }
    } else {
      Usage(argv[0]);
    }
  }
  catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    std::cerr << "ERROR" << std::endl;
    return 1;
  }
  std::cerr << "SUCCESS" << std::endl;
  return 0;
}

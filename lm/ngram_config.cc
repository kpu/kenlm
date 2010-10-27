#include "lm/ngram_config.hh"

#include <iostream>

namespace lm {
namespace ngram {

Config::Config() :
  messages(&std::cerr),
  enumerate_vocab(NULL),
  unknown_missing(COMPLAIN),
  unknown_missing_prob(0.0),
  probing_multiplier(1.5),
  building_memory(1073741824ULL), // 1 GB
  temporary_directory_prefix(NULL),
  arpa_complain(ALL),
  write_mmap(NULL),
  include_vocab(true),
  prefault(false) {}

} // namespace ngram
} // namespace lm

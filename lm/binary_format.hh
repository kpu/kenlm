#ifndef LM_BINARY_FORMAT__
#define LM_BINARY_FORMAT__

#include "lm/ngram_config.hh"
#include "lm/read_arpa.hh"

#include "util/file_piece.hh"
#include "util/mmap.hh"
#include "util/scoped.hh"

#include <cstddef>
#include <vector>

#include <inttypes.h>

namespace lm {
namespace ngram {

typedef enum {HASH_PROBING=0, HASH_SORTED=1, TRIE_SORTED=2} ModelType;

struct FixedWidthParameters {
  unsigned char order;
  float probing_multiplier;
  // What type of model is this?  
  ModelType model_type;
  // Does the end of the file have the actual strings in the vocabulary?   
  bool has_vocabulary;
};

struct Parameters {
  FixedWidthParameters fixed;
  std::vector<uint64_t> counts;
};

struct Backing {
  // File behind memory, if any.  
  util::scoped_fd file;
  // Raw block of memory backing the language model data structures
  util::scoped_mmap memory;
};

namespace detail {

bool IsBinaryFormat(int fd);

uint8_t *SetupBinary(const Config &config, ModelType model_type, Parameters &params, Backing &backing);

uint8_t *SetupZeroed(const Config &config, ModelType model_type, const std::vector<uint64_t> &counts, std::size_t memory_size, Backing &backing);

void ComplainAboutARPA(const Config &config, ModelType model_type);

} // namespace detail

template <class To> void LoadLM(const char *file, const Config &config, To &to) {
  Backing &backing = to.MutableBacking();
  backing.file.reset(util::OpenReadOrThrow(file));

  Parameters params;

  if (detail::IsBinaryFormat(backing.file.get())) {
    uint8_t *start = detail::SetupBinary(config, To::kModelType, params, backing);
    std::size_t memory_size = To::Size(params.counts, config);
    if (static_cast<ptrdiff_t>(memory_size) != backing.memory.end() - start)
      UTIL_THROW(FormatLoadException, "The mmap file " << file << " has size " << backing.memory.size() << " but " << (memory_size + start - backing.memory.begin()) << " was expected based on the counts and configuration.");
    to.InitializeFromBinary(start, params, config);
  } else {
    detail::ComplainAboutARPA(config, To::kModelType);
    util::FilePiece f(backing.file.release(), file, config.messages);
    ReadARPACounts(f, params.counts);
    uint8_t *start = detail::SetupZeroed(config, To::kModelType, params.counts, To::Size(params.counts, config), backing);

    try {
      to.InitializeFromARPA(file, f, start, params, config);
    } catch (FormatLoadException &e) {
      e << " in file " << file;
      throw;
    }
  }
}

} // namespace ngram
} // namespace lm
#endif // LM_BINARY_FORMAT__

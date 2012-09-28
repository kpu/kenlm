#ifndef LM_BINARY_FORMAT__
#define LM_BINARY_FORMAT__

#include "lm/config.hh"
#include "lm/model_type.hh"
#include "lm/read_arpa.hh"

#include "util/file_piece.hh"
#include "util/mmap.hh"
#include "util/scoped.hh"

#include <cstddef>
#include <vector>

#include <stdint.h>

namespace lm {
namespace ngram {

/*Inspect a file to determine if it is a binary lm.  If not, return false.  
 * If so, return true and set recognized to the type.  This is the only API in
 * this header designed for use by decoder authors.  
 */
bool RecognizeBinary(const char *file, ModelType &recognized);

struct FixedWidthParameters {
  unsigned char order;
  float probing_multiplier;
  // What type of model is this?  
  ModelType model_type;
  // Does the end of the file have the actual strings in the vocabulary?   
  bool has_vocabulary;
  unsigned int search_version;
};

// This is a macro instead of an inline function so constants can be assigned using it.
#define ALIGN8(a) ((std::ptrdiff_t(((a)-1)/8)+1)*8)

// Parameters stored in the header of a binary file.  
struct Parameters {
  FixedWidthParameters fixed;
  std::vector<uint64_t> counts;
};

struct Backing {
  // File behind memory, if any.  
  util::scoped_fd file;
  // Vocabulary lookup table.  Not to be confused with the vocab words themselves.  
  util::scoped_memory vocab;
  // Raw block of memory backing the language model data structures
  util::scoped_memory search;
};

// Create just enough of a binary file to write vocabulary to it.  
uint8_t *SetupJustVocab(const Config &config, uint8_t order, std::size_t memory_size, Backing &backing);
// Grow the binary file for the search data structure and set backing.search, returning the memory address where the search data structure should begin.  
uint8_t *GrowForSearch(const Config &config, std::size_t vocab_pad, std::size_t memory_size, Backing &backing);

// Write header to binary file.  This is done last to prevent incomplete files
// from loading.   
void FinishFile(const Config &config, ModelType model_type, unsigned int search_version, const std::vector<uint64_t> &counts,  std::size_t vocab_pad, Backing &backing);

namespace detail {

bool IsBinaryFormat(int fd);

void ReadHeader(int fd, Parameters &params);

void MatchCheck(ModelType model_type, unsigned int search_version, const Parameters &params);

void SeekPastHeader(int fd, const Parameters &params);

uint8_t *SetupBinary(const Config &config, const Parameters &params, uint64_t memory_size, Backing &backing);

void ComplainAboutARPA(const Config &config, ModelType model_type);

} // namespace detail

template <class To> void LoadLM(const char *file, const Config &config, To &to) {
  Backing &backing = to.MutableBacking();
  backing.file.reset(util::OpenReadOrThrow(file));

  try {
    if (detail::IsBinaryFormat(backing.file.get())) {
      Parameters params;
      detail::ReadHeader(backing.file.get(), params);
      detail::MatchCheck(To::kModelType, To::kVersion, params);
      // Replace the run-time configured probing_multiplier with the one in the file.  
      Config new_config(config);
      new_config.probing_multiplier = params.fixed.probing_multiplier;
      detail::SeekPastHeader(backing.file.get(), params);
      To::UpdateConfigFromBinary(backing.file.get(), params.counts, new_config);
      uint64_t memory_size = To::Size(params.counts, new_config);
      uint8_t *start = detail::SetupBinary(new_config, params, memory_size, backing);
      to.InitializeFromBinary(start, params, new_config, backing.file.get());
    } else {
      detail::ComplainAboutARPA(config, To::kModelType);
      to.InitializeFromARPA(file, config);
    }
  } catch (util::Exception &e) {
    e << " File: " << file;
    throw;
  }
}

} // namespace ngram
} // namespace lm
#endif // LM_BINARY_FORMAT__

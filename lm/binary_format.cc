#include "lm/binary_format.hh"

#include "lm/lm_exception.hh"
#include "util/file_piece.hh"

#include <limits>
#include <string>

#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace lm {
namespace ngram {
namespace {
const char kMagicBeforeVersion[] = "mmap lm http://kheafield.com/code format version";
const char kMagicBytes[] = "mmap lm http://kheafield.com/code format version 1\n\0";
const long int kMagicVersion = 1;

// Test values.  
struct Sanity {
  char magic[sizeof(kMagicBytes)];
  float zero_f, one_f, minus_half_f;
  WordIndex one_word_index, max_word_index;
  uint64_t one_uint64;

  void SetToReference() {
    std::memcpy(magic, kMagicBytes, sizeof(magic));
    zero_f = 0.0; one_f = 1.0; minus_half_f = -0.5;
    one_word_index = 1;
    max_word_index = std::numeric_limits<WordIndex>::max();
    one_uint64 = 1;
  }
};

const char *kModelNames[3] = {"hashed n-grams with probing", "hashed n-grams with sorted uniform find", "bit packed trie"};

std::size_t Align8(std::size_t in) {
  std::size_t off = in % 8;
  if (!off) return in;
  return in + 8 - off;
}

std::size_t TotalHeaderSize(unsigned char order) {
  return Align8(sizeof(Sanity) + sizeof(FixedWidthParameters) + sizeof(uint64_t) * order);
}

void ReadHeader(const void *from, off_t size, Parameters &out) {
  const char *from_char = reinterpret_cast<const char*>(from);
  // Skip over Sanity which was read by IsBinaryFormat.  
  from_char += sizeof(Sanity);

  if (size < static_cast<off_t>(sizeof(Sanity) + sizeof(FixedWidthParameters))) 
    UTIL_THROW(FormatLoadException, "File too short to have count information.");
  out.fixed = *reinterpret_cast<const FixedWidthParameters*>(from_char);
  from_char += sizeof(FixedWidthParameters);

  if (size < static_cast<off_t>(TotalHeaderSize(out.fixed.order)))
    UTIL_THROW(FormatLoadException, "File too short to have full header.");
  out.counts.resize(static_cast<std::size_t>(out.fixed.order));
  const uint64_t *counts = reinterpret_cast<const uint64_t*>(from_char);
  for (std::size_t i = 0; i < out.counts.size(); ++i) {
    out.counts[i] = counts[i];
  }
}

void WriteHeader(void *to, const Parameters &params) {
  Sanity header = Sanity();
  header.SetToReference();
  memcpy(to, &header, sizeof(Sanity));
  char *out = reinterpret_cast<char*>(to) + sizeof(Sanity);

  *reinterpret_cast<FixedWidthParameters*>(out) = params.fixed;
  out += sizeof(FixedWidthParameters);

  uint64_t *counts = reinterpret_cast<uint64_t*>(out);
  for (std::size_t i = 0; i < params.counts.size(); ++i) {
    counts[i] = params.counts[i];
  }
}

} // namespace
namespace detail {

bool IsBinaryFormat(int fd) {
  const off_t size = util::SizeFile(fd);
  if (size == util::kBadSize || (size <= static_cast<off_t>(sizeof(Sanity)))) return false;
  // Try reading the header.  
  util::scoped_mmap memory(mmap(NULL, sizeof(Sanity), PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0), sizeof(Sanity));
  if (memory.get() == MAP_FAILED) return false;
  Sanity reference_header = Sanity();
  reference_header.SetToReference();
  if (!memcmp(memory.get(), &reference_header, sizeof(Sanity))) return true;
  if (!memcmp(memory.get(), kMagicBeforeVersion, strlen(kMagicBeforeVersion))) {
    char *end_ptr;
    const char *begin_version = static_cast<const char*>(memory.get()) + strlen(kMagicBeforeVersion);
    long int version = strtol(begin_version, &end_ptr, 10);
    if ((end_ptr != begin_version) && version != kMagicVersion) {
      UTIL_THROW(FormatLoadException, "Binary file has version " << version << " but this implementation expects version " << kMagicVersion << " so you'll have to rebuild your binary LM from the ARPA.  Sorry.");
    }
    UTIL_THROW(FormatLoadException, "File looks like it should be loaded with mmap, but the test values don't match.  Try rebuilding the binary format LM using the same code revision, compiler, and architecture.");
  }
  return false;
}

uint8_t *SetupBinary(const Config &config, ModelType model_type, Parameters &params, Backing &backing) {
  const off_t file_size = util::SizeFile(backing.file.get());
  backing.memory.reset(util::MapForRead(file_size, config.prefault, backing.file.get()), file_size);

  ReadHeader(backing.memory.get(), file_size, params);
  if (params.fixed.probing_multiplier < 1.0)
    UTIL_THROW(FormatLoadException, "Binary format claims to have a probing multiplier of " << params.fixed.probing_multiplier << " which is < 1.0.");
  if (params.fixed.model_type != model_type) {
    if (static_cast<unsigned int>(params.fixed.model_type) >= (sizeof(kModelNames) / sizeof(const char *)))
      UTIL_THROW(FormatLoadException, "The binary file claims to be model type " << static_cast<unsigned int>(params.fixed.model_type) << " but this is not implemented for in this inference code.");
    UTIL_THROW(FormatLoadException, "The binary file was built for " << kModelNames[params.fixed.model_type] << " but the inference code is trying to load " << kModelNames[model_type]);
  }
  return reinterpret_cast<uint8_t*>(backing.memory.get()) + TotalHeaderSize(params.counts.size());
}

uint8_t *SetupZeroed(const Config &config, ModelType model_type, const std::vector<uint64_t> &counts, std::size_t memory_size, Backing &backing) {
  if (config.probing_multiplier <= 1.0) UTIL_THROW(FormatLoadException, "probing multiplier must be > 1.0");
  if (config.write_mmap) {
    // Write out an mmap file.  
    util::MapZeroedWrite(config.write_mmap, TotalHeaderSize(counts.size()) + memory_size, backing.file, backing.memory);

    Parameters params;
    params.counts = counts;
    params.fixed.order = counts.size();
    params.fixed.probing_multiplier = config.probing_multiplier;
    params.fixed.model_type = model_type;
    params.fixed.has_vocabulary = false;

    WriteHeader(backing.memory.get(), params);
    return reinterpret_cast<uint8_t*>(backing.memory.get()) + TotalHeaderSize(counts.size());
  } else {
    backing.memory.reset(util::MapAnonymous(memory_size), memory_size);
    return reinterpret_cast<uint8_t*>(backing.memory.get());
  } 
}

void SizeCheck(std::size_t expected, const uint8_t *start, const Backing &backing) {
  if (expected < static_cast<std::size_t>(backing.memory.end() - start)) {
    UTIL_THROW(FormatLoadException, "Binary file should have size >=" << (expected + start - backing.memory.begin()) << " based on the counts and configuration.");
  }
}

void ComplainAboutARPA(const Config &config, ModelType model_type) {
  if (config.write_mmap) return;
  if (config.arpa_complain == Config::ALL) {
    std::cerr << "Loading the LM will be faster if you build a binary file." << std::endl;
  } else if (config.arpa_complain == Config::EXPENSIVE && model_type == TRIE_SORTED) {
    std::cerr << "Building " << kModelNames[model_type] << " from ARPA is expensive.  Save time by building a binary format." << std::endl;
  }
}

} // namespace detail
} // namespace ngram
} // namespace lm

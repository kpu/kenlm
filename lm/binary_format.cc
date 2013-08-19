#include "lm/binary_format.hh"

#include "lm/lm_exception.hh"
#include "util/file.hh"
#include "util/file_piece.hh"

#include <cstddef>
#include <cstring>
#include <limits>
#include <string>
#include <cstdlib>

#include <stdint.h>

namespace lm {
namespace ngram {
namespace {
const char kMagicBeforeVersion[] = "mmap lm http://kheafield.com/code format version";
const char kMagicBytes[] = "mmap lm http://kheafield.com/code format version 5\n\0";
// This must be shorter than kMagicBytes and indicates an incomplete binary file (i.e. build failed).
const char kMagicIncomplete[] = "mmap lm http://kheafield.com/code incomplete\n";
const long int kMagicVersion = 5;

// Old binary files built on 32-bit machines have this header.
// TODO: eliminate with next binary release.
struct OldSanity {
  char magic[sizeof(kMagicBytes)];
  float zero_f, one_f, minus_half_f;
  WordIndex one_word_index, max_word_index;
  uint64_t one_uint64;

  void SetToReference() {
    std::memset(this, 0, sizeof(OldSanity));
    std::memcpy(magic, kMagicBytes, sizeof(magic));
    zero_f = 0.0; one_f = 1.0; minus_half_f = -0.5;
    one_word_index = 1;
    max_word_index = std::numeric_limits<WordIndex>::max();
    one_uint64 = 1;
  }
};


// Test values aligned to 8 bytes.
struct Sanity {
  char magic[ALIGN8(sizeof(kMagicBytes))];
  float zero_f, one_f, minus_half_f;
  WordIndex one_word_index, max_word_index, padding_to_8;
  uint64_t one_uint64;

  void SetToReference() {
    std::memset(this, 0, sizeof(Sanity));
    std::memcpy(magic, kMagicBytes, sizeof(kMagicBytes));
    zero_f = 0.0; one_f = 1.0; minus_half_f = -0.5;
    one_word_index = 1;
    max_word_index = std::numeric_limits<WordIndex>::max();
    padding_to_8 = 0;
    one_uint64 = 1;
  }
};

const char *kModelNames[6] = {"probing hash tables", "probing hash tables with rest costs", "trie", "trie with quantization", "trie with array-compressed pointers", "trie with quantization and array-compressed pointers"};

std::size_t TotalHeaderSize(unsigned char order) {
  return ALIGN8(sizeof(Sanity) + sizeof(FixedWidthParameters) + sizeof(uint64_t) * order);
}

void WriteHeader(void *to, const Parameters &params) {
  Sanity header = Sanity();
  header.SetToReference();
  std::memcpy(to, &header, sizeof(Sanity));
  char *out = reinterpret_cast<char*>(to) + sizeof(Sanity);

  *reinterpret_cast<FixedWidthParameters*>(out) = params.fixed;
  out += sizeof(FixedWidthParameters);

  uint64_t *counts = reinterpret_cast<uint64_t*>(out);
  for (std::size_t i = 0; i < params.counts.size(); ++i) {
    counts[i] = params.counts[i];
  }
}

} // namespace

uint8_t *SetupJustVocab(const Config &config, uint8_t order, std::size_t memory_size, Backing &backing) {
  if (config.write_mmap) {
    std::size_t total = TotalHeaderSize(order) + memory_size;
    backing.file.reset(util::CreateOrThrow(config.write_mmap));
    if (config.write_method == Config::WRITE_MMAP) {
      backing.vocab.reset(util::MapZeroedWrite(backing.file.get(), total), total, util::scoped_memory::MMAP_ALLOCATED);
    } else {
      util::ResizeOrThrow(backing.file.get(), 0);
      util::MapAnonymous(total, backing.vocab);
    }
    strncpy(reinterpret_cast<char*>(backing.vocab.get()), kMagicIncomplete, TotalHeaderSize(order));
    return reinterpret_cast<uint8_t*>(backing.vocab.get()) + TotalHeaderSize(order);
  } else {
    util::MapAnonymous(memory_size, backing.vocab);
    return reinterpret_cast<uint8_t*>(backing.vocab.get());
  }
}

uint8_t *GrowForSearch(const Config &config, std::size_t vocab_pad, std::size_t memory_size, Backing &backing) {
  std::size_t adjusted_vocab = backing.vocab.size() + vocab_pad;
  if (config.write_mmap) {
    // Grow the file to accomodate the search, using zeros.
    try {
      util::ResizeOrThrow(backing.file.get(), adjusted_vocab + memory_size);
    } catch (util::ErrnoException &e) {
      e << " for file " << config.write_mmap;
      throw e;
    }

    if (config.write_method == Config::WRITE_AFTER) {
      util::MapAnonymous(memory_size, backing.search);
      return reinterpret_cast<uint8_t*>(backing.search.get());
    }
    // mmap it now.
    // We're skipping over the header and vocab for the search space mmap.  mmap likes page aligned offsets, so some arithmetic to round the offset down.
    std::size_t page_size = util::SizePage();
    std::size_t alignment_cruft = adjusted_vocab % page_size;
    backing.search.reset(util::MapOrThrow(alignment_cruft + memory_size, true, util::kFileFlags, false, backing.file.get(), adjusted_vocab - alignment_cruft), alignment_cruft + memory_size, util::scoped_memory::MMAP_ALLOCATED);
    return reinterpret_cast<uint8_t*>(backing.search.get()) + alignment_cruft;
  } else {
    util::MapAnonymous(memory_size, backing.search);
    return reinterpret_cast<uint8_t*>(backing.search.get());
  }
}

void FinishFile(const Config &config, ModelType model_type, unsigned int search_version, const std::vector<uint64_t> &counts, std::size_t vocab_pad, Backing &backing) {
  if (!config.write_mmap) return;
  switch (config.write_method) {
    case Config::WRITE_MMAP:
      util::SyncOrThrow(backing.vocab.get(), backing.vocab.size());
      util::SyncOrThrow(backing.search.get(), backing.search.size());
      break;
    case Config::WRITE_AFTER:
      util::SeekOrThrow(backing.file.get(), 0);
      util::WriteOrThrow(backing.file.get(), backing.vocab.get(), backing.vocab.size());
      util::SeekOrThrow(backing.file.get(), backing.vocab.size() + vocab_pad);
      util::WriteOrThrow(backing.file.get(), backing.search.get(), backing.search.size());
      util::FSyncOrThrow(backing.file.get());
      break;
  }
  // header and vocab share the same mmap.  The header is written here because we know the counts.
  Parameters params = Parameters();
  params.counts = counts;
  params.fixed.order = counts.size();
  params.fixed.probing_multiplier = config.probing_multiplier;
  params.fixed.model_type = model_type;
  params.fixed.has_vocabulary = config.include_vocab;
  params.fixed.search_version = search_version;
  WriteHeader(backing.vocab.get(), params);
  if (config.write_method == Config::WRITE_AFTER) {
    util::SeekOrThrow(backing.file.get(), 0);
    util::WriteOrThrow(backing.file.get(), backing.vocab.get(), TotalHeaderSize(counts.size()));
  }
}

namespace detail {

bool IsBinaryFormat(int fd) {
  const uint64_t size = util::SizeFile(fd);
  if (size == util::kBadSize || (size <= static_cast<uint64_t>(sizeof(Sanity)))) return false;
  // Try reading the header.
  util::scoped_memory memory;
  try {
    util::MapRead(util::LAZY, fd, 0, sizeof(Sanity), memory);
  } catch (const util::Exception &e) {
    return false;
  }
  Sanity reference_header = Sanity();
  reference_header.SetToReference();
  if (!std::memcmp(memory.get(), &reference_header, sizeof(Sanity))) return true;
  if (!std::memcmp(memory.get(), kMagicIncomplete, strlen(kMagicIncomplete))) {
    UTIL_THROW(FormatLoadException, "This binary file did not finish building");
  }
  if (!std::memcmp(memory.get(), kMagicBeforeVersion, strlen(kMagicBeforeVersion))) {
    char *end_ptr;
    const char *begin_version = static_cast<const char*>(memory.get()) + strlen(kMagicBeforeVersion);
    long int version = std::strtol(begin_version, &end_ptr, 10);
    if ((end_ptr != begin_version) && version != kMagicVersion) {
      UTIL_THROW(FormatLoadException, "Binary file has version " << version << " but this implementation expects version " << kMagicVersion << " so you'll have to use the ARPA to rebuild your binary");
    }

    OldSanity old_sanity = OldSanity();
    old_sanity.SetToReference();
    UTIL_THROW_IF(!std::memcmp(memory.get(), &old_sanity, sizeof(OldSanity)), FormatLoadException, "Looks like this is an old 32-bit format.  The old 32-bit format has been removed so that 64-bit and 32-bit files are exchangeable.");
    UTIL_THROW(FormatLoadException, "File looks like it should be loaded with mmap, but the test values don't match.  Try rebuilding the binary format LM using the same code revision, compiler, and architecture");
  }
  return false;
}

void ReadHeader(int fd, Parameters &out) {
  util::SeekOrThrow(fd, sizeof(Sanity));
  util::ReadOrThrow(fd, &out.fixed, sizeof(out.fixed));
  if (out.fixed.probing_multiplier < 1.0)
    UTIL_THROW(FormatLoadException, "Binary format claims to have a probing multiplier of " << out.fixed.probing_multiplier << " which is < 1.0.");

  out.counts.resize(static_cast<std::size_t>(out.fixed.order));
  if (out.fixed.order) util::ReadOrThrow(fd, &*out.counts.begin(), sizeof(uint64_t) * out.fixed.order);
}

void MatchCheck(ModelType model_type, unsigned int search_version, const Parameters &params) {
  if (params.fixed.model_type != model_type) {
    if (static_cast<unsigned int>(params.fixed.model_type) >= (sizeof(kModelNames) / sizeof(const char *)))
      UTIL_THROW(FormatLoadException, "The binary file claims to be model type " << static_cast<unsigned int>(params.fixed.model_type) << " but this is not implemented for in this inference code.");
    UTIL_THROW(FormatLoadException, "The binary file was built for " << kModelNames[params.fixed.model_type] << " but the inference code is trying to load " << kModelNames[model_type]);
  }
  UTIL_THROW_IF(search_version != params.fixed.search_version, FormatLoadException, "The binary file has " << kModelNames[params.fixed.model_type] << " version " << params.fixed.search_version << " but this code expects " << kModelNames[params.fixed.model_type] << " version " << search_version);
}

void SeekPastHeader(int fd, const Parameters &params) {
  util::SeekOrThrow(fd, TotalHeaderSize(params.counts.size()));
}

uint8_t *SetupBinary(const Config &config, const Parameters &params, uint64_t memory_size, Backing &backing) {
  const uint64_t file_size = util::SizeFile(backing.file.get());
  // The header is smaller than a page, so we have to map the whole header as well.
  std::size_t total_map = util::CheckOverflow(TotalHeaderSize(params.counts.size()) + memory_size);
  if (file_size != util::kBadSize && static_cast<uint64_t>(file_size) < total_map)
    UTIL_THROW(FormatLoadException, "Binary file has size " << file_size << " but the headers say it should be at least " << total_map);

  util::MapRead(config.load_method, backing.file.get(), 0, total_map, backing.search);

  if (config.enumerate_vocab && !params.fixed.has_vocabulary)
    UTIL_THROW(FormatLoadException, "The decoder requested all the vocabulary strings, but this binary file does not have them.  You may need to rebuild the binary file with an updated version of build_binary.");

  // Seek to vocabulary words
  util::SeekOrThrow(backing.file.get(), total_map);
  return reinterpret_cast<uint8_t*>(backing.search.get()) + TotalHeaderSize(params.counts.size());
}

void ComplainAboutARPA(const Config &config, ModelType model_type) {
  if (config.write_mmap || !config.messages) return;
  if (config.arpa_complain == Config::ALL) {
    *config.messages << "Loading the LM will be faster if you build a binary file." << std::endl;
  } else if (config.arpa_complain == Config::EXPENSIVE &&
             (model_type == TRIE || model_type == QUANT_TRIE || model_type == ARRAY_TRIE || model_type == QUANT_ARRAY_TRIE)) {
    *config.messages << "Building " << kModelNames[model_type] << " from ARPA is expensive.  Save time by building a binary format." << std::endl;
  }
}

} // namespace detail

bool RecognizeBinary(const char *file, ModelType &recognized) {
  util::scoped_fd fd(util::OpenReadOrThrow(file));
  if (!detail::IsBinaryFormat(fd.get())) return false;
  Parameters params;
  detail::ReadHeader(fd.get(), params);
  recognized = params.fixed.model_type;
  return true;
}

} // namespace ngram
} // namespace lm

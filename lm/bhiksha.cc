#include "lm/bhiksha.hh"
#include "lm/config.hh"
#include "util/file.hh"
#include "util/exception.hh"

#include <limits>

namespace lm {
namespace ngram {
namespace trie {

DontBhiksha::DontBhiksha(const util::Rolling & /*base*/, uint64_t /*max_offset*/, uint64_t max_next, const Config &/*config*/) : 
  next_(util::BitsMask::ByMax(max_next)) {}

const uint8_t kArrayBhikshaVersion = 0;

// TODO: put this in binary file header instead when I change the binary file format again.  
void ArrayBhiksha::UpdateConfigFromBinary(int fd, Config &config) {
  uint8_t version;
  uint8_t configured_bits;
  util::ReadOrThrow(fd, &version, 1);
  util::ReadOrThrow(fd, &configured_bits, 1);
  if (version != kArrayBhikshaVersion) UTIL_THROW(FormatLoadException, "This file has sorted array compression version " << (unsigned) version << " but the code expects version " << (unsigned)kArrayBhikshaVersion);
  config.pointer_bhiksha_bits = configured_bits;
}

namespace {

// Find argmin_{chopped \in [0, RequiredBits(max_next)]} ChoppedDelta(max_offset)
uint8_t ChopBits(uint64_t max_offset, uint64_t max_next, const Config &config) {
  uint8_t required = util::RequiredBits(max_next);
  uint8_t best_chop = 0;
  int64_t lowest_change = std::numeric_limits<int64_t>::max();
  // There are probably faster ways but I don't care because this is only done once per order at construction time.  
  for (uint8_t chop = 0; chop <= std::min(required, config.pointer_bhiksha_bits); ++chop) {
    int64_t change = (max_next >> (required - chop)) * 64 /* table cost in bits */
      - max_offset * static_cast<int64_t>(chop); /* savings in bits*/
    if (change < lowest_change) {
      lowest_change = change;
      best_chop = chop;
    }
  }
  return best_chop;
}

std::size_t ArrayCount(uint64_t max_offset, uint64_t max_next, const Config &config) {
  uint8_t required = util::RequiredBits(max_next);
  uint8_t chopping = ChopBits(max_offset, max_next, config);
  return (max_next >> (required - chopping)) + 1 /* we store 0 too */;
}
} // namespace

uint64_t ArrayBhiksha::Size(uint64_t max_offset, uint64_t max_next, const Config &config) {
  return sizeof(uint64_t) * (1 /* header */ + ArrayCount(max_offset, max_next, config)) + 7 /* 8-byte alignment */;
}

uint8_t ArrayBhiksha::InlineBits(uint64_t max_offset, uint64_t max_next, const Config &config) {
  return util::RequiredBits(max_next) - ChopBits(max_offset, max_next, config);
}

namespace {

std::size_t AlignTo8(void *from) {
  uint8_t *val = reinterpret_cast<uint8_t*>(from);
  std::size_t remainder = reinterpret_cast<std::size_t>(val) & 7;
  if (!remainder) return 0;
  return 8 - remainder;
}

} // namespace

ArrayBhiksha::ArrayBhiksha(const util::Rolling &mem, uint64_t max_offset, uint64_t max_next, const Config &config)
  : next_inline_(util::BitsMask::ByBits(InlineBits(max_offset, max_next, config))),
    mem_(mem, AlignTo8(mem.get())),
    offset_begin_(static_cast<uint64_t*>(mem_.CheckedIndex(8 /* 8-byte header */))),
    offset_end_(offset_begin_ + ArrayCount(max_offset, max_next, config)),
    write_index_(16),
    aligned_(AlignTo8(mem.get())) {} // 8-byte header and first entry is 0.

void ArrayBhiksha::FinishedLoading(const Config &config) {
  mem_.DecreaseBase(aligned_);
  // Two byte header padded to 8 bytes.
  uint8_t *head_write = reinterpret_cast<uint8_t*>(mem_.CheckedIndex(0));
  *(head_write++) = kArrayBhikshaVersion;
  *(head_write++) = config.pointer_bhiksha_bits;
  // First entry is 0.
  *static_cast<uint64_t*>(mem_.CheckedIndex(8 + aligned_)) = 0;
}

} // namespace trie
} // namespace ngram
} // namespace lm

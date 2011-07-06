#include "lm/bhiksha.hh"
#include "lm/config.hh"

namespace lm {
namespace ngram {
namespace trie {

DontBhiksha::DontBhiksha(const void * /*base*/, uint64_t /*max_offset*/, uint64_t max_next, const Config &/*config*/) : 
  next_(util::BitsMask::ByMax(max_next)) {}

const uint8_t kArrayBhikshaVersion = 0;

void ArrayBhiksha::UpdateConfigFromBinary(int fd, Config &config) {
  uint8_t version;
  uint8_t configured_bits;
  if (read(fd, &version, 1) != 1 || read(fd, &configured_bits, 1) != 1) {
    UTIL_THROW(util::ErrnoException, "Could not read from binary file");
  }
  if (version != kArrayBhikshaVersion) UTIL_THROW(FormatLoadException, "This file has sorted array compression version " << (unsigned) version << " but the code expects version " << (unsigned)kArrayBhikshaVersion);
  config.pointer_bhiksha_bits = configured_bits;
}

namespace {
std::size_t ArrayCount(uint64_t max_next, const Config &config) {
  uint8_t required = util::RequiredBits(max_next);
  uint8_t storing = std::min(required, config.pointer_bhiksha_bits);
  return ((max_next >> (required - storing)) + 1 /* we store 0 too */);
}
} // namespace

std::size_t ArrayBhiksha::Size(uint64_t /*max_offset*/, uint64_t max_next, const Config &config) {
  return sizeof(uint64_t) * (1 /* header */ + ArrayCount(max_next, config)) + 7 /* 8-byte alignment */;
}

uint8_t ArrayBhiksha::InlineBits(uint64_t /*max_offset*/, uint64_t max_next, const Config &config) {
  uint8_t total = util::RequiredBits(max_next);
  return (total > config.pointer_bhiksha_bits) ? (total - config.pointer_bhiksha_bits) : 0;
}

namespace {

void *AlignTo8(void *from) {
  uint8_t *val = reinterpret_cast<uint8_t*>(from);
  std::size_t remainder = reinterpret_cast<std::size_t>(val) & 7;
  if (!remainder) return val;
  return val + 8 - remainder;
}

} // namespace

ArrayBhiksha::ArrayBhiksha(void *base, uint64_t max_offset, uint64_t max_next, const Config &config)
  : next_inline_(util::BitsMask::ByBits(InlineBits(max_offset, max_next, config))),
    offset_begin_(reinterpret_cast<const uint64_t*>(AlignTo8(base)) + 1 /* 8-byte header */),
    offset_end_(offset_begin_ + ArrayCount(max_next, config)),
    write_to_(reinterpret_cast<uint64_t*>(AlignTo8(base)) + 1 /* 8-byte header */ + 1 /* first entry is 0 */),
    original_base_(base) {}

void ArrayBhiksha::FinishedLoading(const Config &config) {
  // *offset_begin_ = 0 but without a const_cast.
  *(write_to_ - (write_to_ - offset_begin_)) = 0;

  if (write_to_ != offset_end_) UTIL_THROW(util::Exception, "Did not get all the array entries that were expected.");

  offset_bound_ = *(offset_end_ - 1);
  uint8_t *head_write = reinterpret_cast<uint8_t*>(original_base_);
  *(head_write++) = kArrayBhikshaVersion;
  *(head_write++) = config.pointer_bhiksha_bits;
}

void ArrayBhiksha::LoadedBinary() {
  offset_bound_ = *(offset_end_ - 1);
}

} // namespace trie
} // namespace ngram
} // namespace lm

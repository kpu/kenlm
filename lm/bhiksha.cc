#include "lm/bhiksha.hh"
#include "lm/config.hh"

namespace lm {
namespace ngram {
namespace trie {

DontBhiksha::DontBhiksha(const void * /*base*/, uint64_t /*max_offset*/, uint64_t max_next, const Config &/*config*/) : 
  next_(util::BitsMask::ByMax(max_next)) {}

std::size_t ArrayBhiksha::Size(uint64_t /*max_offset*/, uint64_t max_next, const Config &config) {
  uint8_t required = util::RequiredBits(max_next);
  uint8_t storing = std::min(required, config.pointer_bhiksha_bits);
  return sizeof(uint64_t) * ((max_next >> (required - storing)) + 1);
}

uint8_t ArrayBhiksha::InlineBits(uint64_t /*max_offset*/, uint64_t max_next, const Config &config) {
  uint8_t total = util::RequiredBits(max_next);
  return (total > config.pointer_bhiksha_bits) ? (total - config.pointer_bhiksha_bits) : 0;
}

ArrayBhiksha::ArrayBhiksha(void *base, uint64_t max_offset, uint64_t max_next, const Config &config)
  : next_inline_(util::BitsMask::ByBits(InlineBits(max_offset, max_next, config))),
    offset_begin_(reinterpret_cast<const uint64_t*>(base)),
    offset_end_(offset_begin_ + Size(max_offset, max_next, config) / sizeof(uint64_t)),
    write_to_(reinterpret_cast<uint64_t*>(base) + 1) {}

void ArrayBhiksha::FinishedLoading() {
  // *offset_begin_ = 0 but without a const_cast.
  *(write_to_ - (write_to_ - offset_begin_)) = 0;

  // Pad remainder.  This should be small.  
  uint64_t pad = *(write_to_ - 1) + 1;
  for (; write_to_ < offset_end_; ++write_to_) *write_to_ = pad;

  offset_bound_ = *(offset_end_ - 1);
}

void ArrayBhiksha::LoadedBinary() {
  offset_bound_ = *(offset_end_ - 1);
}

} // namespace trie
} // namespace ngram
} // namespace lm

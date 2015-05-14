#include "lm/interpolate/bounded_sequence_encoding.hh"

#include <cstring>

namespace lm { namespace interpolate {

BoundedSequenceEncoding::BoundedSequenceEncoding(const unsigned char *bound_begin, const unsigned char *bound_end)
  : entries_(bound_end - bound_begin) {
  Entry entry;
  entry.shift = 0;
  entry.index = 0;
  for (const unsigned char *i = bound_begin; i != bound_end; ++i) {
    uint8_t length;
    if (*i <= 1) {
      length = 0;
    } else {
      length = sizeof(unsigned int) * 8 - __builtin_clz((unsigned int)*i);
    }
    entry.mask = (1ULL << length) - 1ULL;
    if (entry.shift + length > 64) {
      entry.shift = 0;
      ++entry.index;
    }
    entries_.push_back(entry);
    entry.shift += length;
  }
  byte_length_ = entry.index * sizeof(uint64_t) + (entry.shift + 7) / 8;
}

void BoundedSequenceEncoding::Encode(const unsigned char *from, void *to_void) const {
  std::memset(to_void, 0, byte_length_);
  uint64_t *to = static_cast<uint64_t*>(to_void);
  for (const Entry *i = entries_.begin(); i != entries_.end(); ++i, ++from) {
    to[i->index] |= static_cast<uint64_t>(*from) << i->shift;
  }
}

void BoundedSequenceEncoding::Decode(const void *from_void, unsigned char *to) const {
  const uint64_t *from = static_cast<const uint64_t*>(from_void);
  for (const Entry *i = entries_.begin(); i != entries_.end(); ++i, ++to) {
    *to = (from[i->index] >> i->shift) & i->mask;
  }
}

}} // namespaces

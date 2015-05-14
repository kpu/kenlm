#ifndef LM_INTERPOLATE_BOUNDED_SEQUENCE_ENCODING_H
#define LM_INTERPOLATE_BOUNDED_SEQUENCE_ENCODING_H

/* Encodes fixed-length sequences of integers with known bounds on each entry.
 * This is used to encode how far each model has backed off.
 * TODO: make this class efficient.  Bit-level packing or multiply by bound and
 * add.
 */

#include "util/fixed_array.hh"

namespace lm {
namespace interpolate {

class BoundedSequenceEncoding {
  public:
    // Encode [0, bound_begin[0]) x [0, bound_begin[1]) x [0, bound_begin[2]) x ... x [0, *(bound_end - 1)) for entries in the sequence
    BoundedSequenceEncoding(const unsigned char *bound_begin, const unsigned char *bound_end);

    std::size_t EncodedLength() const { return byte_length_; }

    void Encode(const unsigned char *from, void *to) const;

    void Decode(const void *from, unsigned char *to) const;

  private:
    struct Entry {
      std::size_t index;
      uint8_t shift;
      uint64_t mask;
    };
    util::FixedArray<Entry> entries_;
    std::size_t byte_length_;
};


}} // namespaces

#endif // LM_INTERPOLATE_BOUNDED_SEQUENCE_ENCODING_H

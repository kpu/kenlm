#ifndef LM_INTERPOLATE_BOUNDED_SEQUENCE_ENCODING_H
#define LM_INTERPOLATE_BOUNDED_SEQUENCE_ENCODING_H

/* Encodes fixed-length sequences of integers with known bounds on each entry.
 * This is used to encode how far each model has backed off.
 * TODO: make this class efficient.  Bit-level packing or multiply by bound and
 * add.
 */

#include <cstring>

namespace lm {
namespace interpolate {

class BoundedSequenceEncoding {
  public:
    // Define ranges [0, bound_begin), [0, bound_begin + 1), ... for entries in the sequence
    BoundedSequenceEncoding(const unsigned char *bound_begin, const unsigned char *bound_end)
      : length_(bound_end - bound_begin) {}

    std::size_t Length() const { return length_; }

    void Encode(const unsigned char *from, void *to) {
      std::memcpy(to, from, length_);
    }

    void Decode(const void *from, unsigned char *to) {
      std::memcpy(to, from, length_);
    }

  private:
    const std::size_t length_;
};

}} // namespaces

#endif // LM_INTERPOLATE_BOUNDED_SEQUENCE_ENCODING_H

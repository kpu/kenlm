#ifndef LM_INTERPOLATE_BOUNDED_SEQUENCE_ENCODING_H
#define LM_INTERPOLATE_BOUNDED_SEQUENCE_ENCODING_H

/* Encodes fixed-length sequences of integers with known bounds on each entry.
 * This is used to encode how far each model has backed off.
 * TODO: make this class efficient.  Bit-level packing or multiply by bound and
 * add.
 */

#include <cstring>

#if 0
#include "util/bit_packing.hh"
#include "lm/interpolate/bounded_sequence_iterator.hh"
#endif

namespace lm {
namespace interpolate {

class BoundedSequenceEncoding {
  public:
    // Encode [0, bound_begin[0]) x [0, bound_begin[1]) x [0, bound_begin[2]) x ... x [0, *(bound_end - 1)) for entries in the sequence
    BoundedSequenceEncoding(const unsigned char *bound_begin, const unsigned char *bound_end)
      : length_(bound_end - bound_begin) {}

    std::size_t Length() const { return length_; }

    void Encode(const unsigned char *from, void *to) {
      std::memcpy(to, from, length_);
    }

    void Decode(const void *from, unsigned char *to) {
      std::memcpy(to, from, length_);
    }

#if 0
    void Encode(const unsigned char *from, void *to) {

      //TODO: THIS SHOULD BE KNOWN ... SHOULD BE ABLE TO DELETE THIS
      // Figure out max order of language model
      //unsigned char max = 0;
      //BoundedSequenceIterator it_from(from);
      //BoundedSequenceIterator it_until(from + length_); //TODO: BAD ... what should this be
      //for (BoundedSequenceIterator it=it_from; it !=it_from; it++) {
      //  if (*it > max) {
      //    max = *it;
      //  }
      //}

      //TODO: THIS SHOULD ALSO PROBABLY BE KNOWN
      uint64_t bit_off;
      uint8_t length_bits; //TODO: Should be number of bits to store max
      uint8_t num_bits_needed; 
      uint8_t num_bytes_needed;


      std::size_t it_size;
      bit_off = 0;
      length_bits = 4; //TODO: Remove Hardcoded value
      num_bits_needed = length_bits * length_;//TODO: Number of  bits needed
      //num_bytes_needed = math::ceil(num_bits_needed / 25); //TODO: Number of bytes needed
      //Set the memory to 0 ... bitpacking expects it
      memset(to, 0, num_bits_needed);
      BoundedSequenceIteratorEncoder it_from(from);
      BoundedSequenceIteratorEncoder it_until(from + length_); //TODO: BAD ... what should this be
      for (BoundedSequenceIteratorEncoder it=it_from; it !=it_from; it++) {
        it_size = sizeof(*it);
        util::WriteInt25(to, bit_off, length_bits, *from);
        bit_off += num_bits_needed;
        from += it_size;
        //std::memcpy(to, from, it_size);
      }


      /*
      std::size_t it_size;
      uint64_t b = 0;

      BoundedSequenceIterator it_from(from);
      BoundedSequenceIterator it_until(from + length_); //TODO: BAD ... what should this be
      for (BoundedSequenceIterator it=it_from; it !=it_from; it++) {
        it_size = sizeof(*it);
        if (b < 57 - 3) {
          // We need to set everything to 0 as the code expects it to be
          to = to + 64; //it_size; //TODO: not hardcode this
          memset(to, 0, 57); //length_); 
          b = 0;
        }
        util::WriteInt57(to, b, 3, *from); //TODO: figure out if 57 is best
        b += 3;
        from += it_size;
        //std::memcpy(to, from, it_size);
      }*/




      //std::memcpy(to, from, length_);

    }

    //void Decode(const void *from, unsigned char *to) {
    /*BoundedSequenceIteratorDecoder *bsd Decode(const void *from) {


      uint64_t bit_off;
      uint8_t length_bits; //TODO: Should be number of bits to store max
      uint8_t num_bits_needed; 
      uint32_t mask;

      mask = 0x0000010; //Should get the last 4 bits

      std::size_t it_size;
      bit_off = 0;
      length_bits = 4; //TODO: Remove Hardcoded value
      num_bits_needed = length_bits * length_;//TODO: Number of  bits needed
      BoundedSequenceIteratorDecoder it_from(from);
      BoundedSequenceIteratorDecoder it_until(from + length_); //TODO: BAD ... what should this be
      for (BoundedSequenceIteratorDecoder it=it_from; it !=it_from; it++) {
        it_size = sizeof(*it);
        std::memcpy(to, util::ReadInt25(from, bit_off, length_bits, mask), it_size);
        bit_off += num_bits_needed;
        to += it_size;
        //std::memcpy(to, from, it_size);
      }
      


      //std::memcpy(to, from, length_);


      //BoundedSequenceIterator it_from(*from);
      //BoundedSequenceIterator it_until(length_);
      //for (BoundedSequenceIterator it=it_from; it !=it_until; it++) {
      //  std::size_t it_size;
      //  it_size = sizeof(*it);
      //  std::memcpy(to, from, it_size);
      //  to += it_size;
      //  from += it_size;
      //}
    }*/
#endif

  private:
    const std::size_t length_;
};


}} // namespaces

#endif // LM_INTERPOLATE_BOUNDED_SEQUENCE_ENCODING_H

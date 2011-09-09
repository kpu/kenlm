#ifndef UTIL_BIT_PACKING__
#define UTIL_BIT_PACKING__

/* Bit-level packing routines */

#include <assert.h>
#ifdef __APPLE__
#include <architecture/byte_order.h>
#elif __linux__
#include <endian.h>
#else
#include <arpa/nameser_compat.h>
#endif 

#include <inttypes.h>

namespace util {

/* WARNING WARNING WARNING:
 * The write functions assume that memory is zero initially.  This makes them
 * faster and is the appropriate case for mmapped language model construction.
 * These routines assume that unaligned access to uint64_t is fast and that
 * storage is little endian.  This is the case on x86_64.  I'm not sure how 
 * fast unaligned 64-bit access is on x86 but my target audience is large
 * language models for which 64-bit is necessary.  
 *
 * Call the BitPackingSanity function to sanity check.  Calling once suffices,
 * but it may be called multiple times when that's inconvenient.  
 */


// Fun fact: __BYTE_ORDER is wrong on Solaris Sparc, but the version without __ is correct.  
#if BYTE_ORDER == LITTLE_ENDIAN
inline uint8_t BitPackShift(uint8_t bit, uint8_t /*length*/) {
  return bit;
}
#elif BYTE_ORDER == BIG_ENDIAN
inline uint8_t BitPackShift(uint8_t bit, uint8_t length) {
  return 64 - length - bit;
}
#else
#error "Bit packing code isn't written for your byte order."
#endif

inline uint64_t ReadOff(const void *base, uint64_t bit_off) {
  return *reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(base) + (bit_off >> 3));
}

/* Pack integers up to 57 bits using their least significant digits. 
 * The length is specified using mask:
 * Assumes mask == (1 << length) - 1 where length <= 57.   
 */
inline uint64_t ReadInt57(const void *base, uint64_t bit_off, uint8_t length, uint64_t mask) {
  return (ReadOff(base, bit_off) >> BitPackShift(bit_off & 7, length)) & mask;
}
/* Assumes value < (1 << length) and length <= 57.
 * Assumes the memory is zero initially. 
 */
inline void WriteInt57(void *base, uint64_t bit_off, uint8_t length, uint64_t value) {
  *reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(base) + (bit_off >> 3)) |= 
    (value << BitPackShift(bit_off & 7, length));
}

/* Same caveats as above, but for a 25 bit limit. */
inline uint32_t ReadInt25(const void *base, uint64_t bit_off, uint8_t length, uint32_t mask) {
  return (*reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(base) + (bit_off >> 3)) >> BitPackShift(bit_off & 7, length)) & mask;
}

inline void WriteInt25(void *base, uint64_t bit_off, uint8_t length, uint32_t value) {
  *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(base) + (bit_off >> 3)) |= 
    (value << BitPackShift(bit_off & 7, length));
}

typedef union { float f; uint32_t i; } FloatEnc;

inline float ReadFloat32(const void *base, uint64_t bit_off) {
  FloatEnc encoded;
  encoded.i = ReadOff(base, bit_off) >> BitPackShift(bit_off & 7, 32);
  return encoded.f;
}
inline void WriteFloat32(void *base, uint64_t bit_off, float value) {
  FloatEnc encoded;
  encoded.f = value;
  WriteInt57(base, bit_off, 32, encoded.i);
}

const uint32_t kSignBit = 0x80000000;

inline void SetSign(float &to) {
  FloatEnc enc;
  enc.f = to;
  enc.i |= kSignBit;
  to = enc.f;
}

inline void UnsetSign(float &to) {
  FloatEnc enc;
  enc.f = to;
  enc.i &= ~kSignBit;
  to = enc.f;
}

inline float ReadNonPositiveFloat31(const void *base, uint64_t bit_off) {
  FloatEnc encoded;
  encoded.i = ReadOff(base, bit_off) >> BitPackShift(bit_off & 7, 31);
  // Sign bit set means negative.  
  encoded.i |= kSignBit;
  return encoded.f;
}
inline void WriteNonPositiveFloat31(void *base, uint64_t bit_off, float value) {
  FloatEnc encoded;
  encoded.f = value;
  encoded.i &= ~kSignBit;
  WriteInt57(base, bit_off, 31, encoded.i);
}

void BitPackingSanity();

// Return bits required to store integers upto max_value.  Not the most
// efficient implementation, but this is only called a few times to size tries. 
uint8_t RequiredBits(uint64_t max_value);

struct BitsMask {
  static BitsMask ByMax(uint64_t max_value) {
    BitsMask ret;
    ret.FromMax(max_value);
    return ret;
  }
  static BitsMask ByBits(uint8_t bits) {
    BitsMask ret;
    ret.bits = bits;
    ret.mask = (1ULL << bits) - 1;
    return ret;
  }
  void FromMax(uint64_t max_value) {
    bits = RequiredBits(max_value);
    mask = (1ULL << bits) - 1;
  }
  uint8_t bits;
  uint64_t mask;
};

} // namespace util

#endif // UTIL_BIT_PACKING__

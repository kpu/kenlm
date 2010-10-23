#ifndef UTIL_BIT_PACKING__
#define UTIL_BIT_PACKING__

/* Bit-level packing routines */

#include <assert.h>
#include <endian.h>
#include <inttypes.h>

#if __BYTE_ORDER != __LITTLE_ENDIAN
#error The bit aligned storage functions assume little endian architecture
#endif

namespace util {

/* WARNING WARNING WARNING:
 * The write functions assume that memory is zero initially.  This makes them
 * faster and is the appropriate case for mmapped language model construction.
 * These routines assume that unaligned access to uint64_t is fast and that
 * storage is little endian.  This is the case on x86_64.  It may not be the
 * case on 32-bit x86 but my target audience is large language models for which
 * 64-bit is necessary.  
 */

/* Pack integers up to 57 bits using their least significant digits. 
 * The length is specified using mask:
 * Assumes mask == (1 << length) - 1 where length <= 57.   
 */
inline uint64_t ReadInt57(const void *base, uint8_t bit, uint64_t mask) {
  return (*reinterpret_cast<const uint64_t*>(base) >> bit) & mask;
}
/* Assumes value <= mask and mask == (1 << length) - 1 where length <= 57.
 * Assumes the memory is zero initially. 
 */
inline void WriteInt57(void *base, uint8_t bit, uint64_t value) {
  *reinterpret_cast<uint64_t*>(base) |= (value << bit);
}

namespace detail { typedef union { float f; uint32_t i; } FloatEnc; }
inline float ReadFloat32(const void *base, uint8_t bit) {
  detail::FloatEnc encoded;
  encoded.i = ReadInt57(base, bit, (1ULL << 32) - 1ULL);
  return encoded.f;
}
inline void WriteFloat32(void *base, uint8_t bit, float value) {
  detail::FloatEnc encoded;
  encoded.f = value;
  WriteInt57(base, bit, encoded.i);
}

inline float ReadNonPositiveFloat31(const void *base, uint8_t bit) {
  detail::FloatEnc encoded;
  encoded.i = ReadInt57(base, bit, (1ULL << 31) - 1ULL);
  // Sign bit set means negative.  
  encoded.i |= 0x80000000;
  return encoded.f;
}
inline void WriteNonPositiveFloat31(void *base, uint8_t bit, float value) {
  assert(value <= 0.0);
  detail::FloatEnc encoded;
  encoded.f = value;
  encoded.i &= ~0x80000000;
  WriteInt57(base, bit, encoded.i);
}

void BitPackingSanity();

// Return bits required to store integers upto max_value.  Not the most
// efficient implementation, but this is only called a few times to size tries. 
uint8_t RequiredBits(uint64_t max_value);

struct BitsMask {
  void FromMax(uint64_t max_value) {
    bits = RequiredBits(max_value);
    mask = (1 << bits) - 1;
  }
  uint8_t bits;
  uint64_t mask;
};

} // namespace util

#endif // UTIL_BIT_PACKING__

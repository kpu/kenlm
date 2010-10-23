#include "util/bit_packing.hh"
#include "util/exception.hh"

namespace util {

namespace {
template <bool> struct StaticCheck {};
template <> struct StaticCheck<true> { typedef bool StaticAssertionPassed; };

typedef StaticCheck<sizeof(float) == 4>::StaticAssertionPassed FloatSize;

} // namespace

uint8_t RequiredBits(uint64_t max_value) {
  if (!max_value) return 0;
  uint8_t ret = 1;
  while (max_value >>= 1) ++ret;
  return ret;
}

void BitPackingSanity() {
  const detail::FloatEnc neg1 = { -1.0 }, pos1 = { 1.0 };
  if ((neg1.i ^ pos1.i) != 0x80000000) UTIL_THROW(Exception, "Sign bit is not 0x80000000");
  // TODO: more checks.  
}

} // namespace util

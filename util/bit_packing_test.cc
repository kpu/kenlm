#include "util/bit_packing.hh"

#define BOOST_TEST_MODULE BitPackingTest
#include <boost/test/unit_test.hpp>

namespace util {
namespace {

const uint64_t test57 = 0x123456789abcdefULL;

BOOST_AUTO_TEST_CASE(ZeroBit) {
  char mem[16];
  memset(mem, 0, sizeof(mem));
  WriteInt57(mem, 0, test57);
  BOOST_CHECK_EQUAL(test57, ReadInt57(mem, 0, (1ULL << 57) - 1));
}

BOOST_AUTO_TEST_CASE(EachBit) {
  char mem[16];
  for (uint8_t b = 0; b < 8; ++b) {
    memset(mem, 0, sizeof(mem));
    WriteInt57(mem, b, test57);
    BOOST_CHECK_EQUAL(test57, ReadInt57(mem, b, (1ULL << 57) - 1));
  }
}

BOOST_AUTO_TEST_CASE(Consecutive) {
  char mem[57+8];
  memset(mem, 0, sizeof(mem));
  for (uint64_t b = 0; b < 57 * 8; b += 57) {
    WriteInt57(mem + (b / 8), b % 8, test57);
    BOOST_CHECK_EQUAL(test57, ReadInt57(mem + b / 8, b % 8, (1ULL << 57) - 1));
  }
  for (uint64_t b = 0; b < 57 * 8; b += 57) {
    BOOST_CHECK_EQUAL(test57, ReadInt57(mem + b / 8, b % 8, (1ULL << 57) - 1));
  }
}

BOOST_AUTO_TEST_CASE(Sanity) {
  BitPackingSanity();
}

} // namespace
} // namespace util

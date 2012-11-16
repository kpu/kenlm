#include "util/read_compressed.hh"

#include "util/file.hh"
#include "util/have.hh"

#define BOOST_TEST_MODULE ReadCompressedTest
#include <boost/test/unit_test.hpp>
#include <boost/scoped_ptr.hpp>

#include <fstream>
#include <string>

#include <stdlib.h>

namespace util {
namespace {

void ReadLoop(ReadCompressed &reader, void *to_void, std::size_t amount) {
  uint8_t *to = static_cast<uint8_t*>(to_void);
  while (amount) {
    std::size_t ret = reader.Read(to, amount);
    BOOST_REQUIRE(ret);
    to += ret;
    amount -= ret;
  }
}

void TestRandom(const char *compressor) {
  const uint32_t kSize4 = 100000 / 4;
  char name[] = "tempXXXXXX";

  // Write test file.  
  {
    scoped_fd original(mkstemp(name));
    BOOST_REQUIRE(original.get() > 0);
    for (uint32_t i = 0; i < kSize4; ++i) {
      WriteOrThrow(original.get(), &i, sizeof(uint32_t));
    }
  }

  char gzname[] = "tempXXXXXX";
  scoped_fd gzipped(mkstemp(gzname));

  std::string command(compressor);
#ifdef __CYGWIN__
  command += ".exe";
#endif
  command += " <\"";
  command += name;
  command += "\" >\"";
  command += gzname;
  command += "\"";
  BOOST_REQUIRE_EQUAL(0, system(command.c_str()));

  BOOST_CHECK_EQUAL(0, unlink(name));
  BOOST_CHECK_EQUAL(0, unlink(gzname));

  ReadCompressed reader(gzipped.release());
  for (uint32_t i = 0; i < kSize4; ++i) {
    uint32_t got;
    ReadLoop(reader, &got, sizeof(uint32_t));
    BOOST_CHECK_EQUAL(i, got);
  }

  char ignored;
  BOOST_CHECK_EQUAL((std::size_t)0, reader.Read(&ignored, 1));
  // Test double EOF call.
  BOOST_CHECK_EQUAL((std::size_t)0, reader.Read(&ignored, 1));
}

BOOST_AUTO_TEST_CASE(Uncompressed) {
  TestRandom("cat");
}

#ifdef HAVE_ZLIB
BOOST_AUTO_TEST_CASE(ReadGZ) {
  TestRandom("gzip");
}
#endif // HAVE_ZLIB

#ifdef HAVE_BZLIB
BOOST_AUTO_TEST_CASE(ReadBZ) {
  TestRandom("bzip2");
}
#endif // HAVE_BZLIB

#ifdef HAVE_XZLIB
BOOST_AUTO_TEST_CASE(ReadXZ) {
  TestRandom("xz");
}
#endif

} // namespace
} // namespace util

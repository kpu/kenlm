#include "util/read_compressed.hh"

#include "util/file.hh"
#include "util/have.hh"

#define BOOST_TEST_MODULE ReadCompressedTest
#include <boost/test/unit_test.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/random/mersenne_twister.hpp>

#include <fstream>
#include <string>

#include <stdlib.h>

namespace util {
namespace {

void ReadLoop(ReadCompressed *reader, void *to_void, std::size_t amount) {
  uint8_t *to = static_cast<uint8_t*>(to_void);
  while (amount) {
    std::size_t ret = reader->Read(to, amount);
    BOOST_REQUIRE(ret);
    to += ret;
    amount -= ret;
  }
}

#ifdef HAVE_ZLIB
BOOST_AUTO_TEST_CASE(ReadGZ) {
  const std::size_t kSize8 = 1000000 / 8;
  char name[] = "tempXXXXXX";

  // Write test file.  
  {
    scoped_fd original(mkstemp(name));
    BOOST_REQUIRE(original.get() > 0);
    boost::random::mt19937 rng;
    boost::random::uniform_int_distribution<uint64_t> gen(0, std::numeric_limits<uint64_t>::max());
    for (size_t i = 0; i < kSize8; ++i) {
      uint64_t v = gen(rng);
      WriteOrThrow(original.get(), &v, sizeof(uint64_t));
    }
  }

  char gzname[] = "tempXXXXXX";
  scoped_fd gzipped(mkstemp(gzname));

  std::string command("gzip <\"");
  command += name;
  command += "\" >\"";
  command += gzname;
  command += "\"";
  BOOST_REQUIRE_EQUAL(0, system(command.c_str()));

  BOOST_CHECK_EQUAL(0, unlink(name));
  BOOST_CHECK_EQUAL(0, unlink(gzname));

  boost::random::mt19937 rng;
  boost::random::uniform_int_distribution<uint64_t> gen(0, std::numeric_limits<uint64_t>::max());
  boost::scoped_ptr<ReadCompressed> reader(ReadCompressed::Open(gzipped.release()));
  for (size_t i = 0; i < kSize8; ++i) {
    uint64_t got;
    ReadLoop(reader.get(), &got, sizeof(uint64_t));
    BOOST_CHECK_EQUAL(gen(rng), got);
  }

  char ignored;
  BOOST_CHECK_EQUAL(0, reader->Read(&ignored, 1));
  // Test double EOF call.
  BOOST_CHECK_EQUAL(0, reader->Read(&ignored, 1));
}
#endif

} // namespace
} // namespace util

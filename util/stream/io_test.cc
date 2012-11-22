#include "util/stream/io.hh"

#include "util/stream/chain.hh"
#include "util/file.hh"

#define BOOST_TEST_MODULE IOTest
#include <boost/test/unit_test.hpp>

#include <unistd.h>

namespace util { namespace stream { namespace {

BOOST_AUTO_TEST_CASE(CopyFile) {
  util::TempMaker temps("io_test_temp");

  scoped_fd in(temps.Make());
  for (uint64_t i = 0; i < 100000; ++i) {
    WriteOrThrow(in.get(), &i, sizeof(uint64_t));
  }
  SeekOrThrow(in.get(), 0);
  scoped_fd out(temps.Make());

  ChainConfig config;
  config.entry_size = 8;
  config.block_size = 30;
  config.block_count = 10;
  config.queue_length = 2;

  Chain(config) >> Read(in.get()) >> Write(out.get());

  SeekOrThrow(out.get(), 0);
  for (uint64_t i = 0; i < 100000; ++i) {
    uint64_t got;
    ReadOrThrow(out.get(), &got, sizeof(uint64_t));
    BOOST_CHECK_EQUAL(i, got);
  }
}

}}} // namespaces

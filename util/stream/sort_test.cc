#include "util/stream/sort.hh"

#define BOOST_TEST_MODULE SortTest
#include <boost/test/unit_test.hpp>

#include <algorithm>

#include <unistd.h>

namespace util { namespace stream { namespace {

struct CompareUInt64 : public std::binary_function<const void *, const void *, bool> {
  bool operator()(const void *first, const void *second) const {
    return *static_cast<const uint64_t*>(first) < *reinterpret_cast<const uint64_t*>(second);
  }
};

BOOST_AUTO_TEST_CASE(FromShuffled) {
  util::TempMaker temps("sort_test_temp");
  const uint64_t kSize = 10000;
  std::vector<uint64_t> shuffled;
  shuffled.reserve(kSize);
  for (uint64_t i = 0; i < kSize; ++i) {
    shuffled.push_back(i);
  }
  std::random_shuffle(shuffled.begin(), shuffled.end());
  
  ChainConfig config;
  config.entry_size = 8;
  config.block_size = 100;
  config.block_count = 3;
  config.queue_length = 4;
  Sort<CompareUInt64> boss(temps, CompareUInt64());
  {
    Chain chain(config);
    Stream put_shuffled(chain.Between(), true);
    SortChain<CompareUInt64> sorter(chain, boss);
    for (uint64_t i = 0; i < kSize; ++i, ++put_shuffled) {
      *static_cast<uint64_t*>(put_shuffled.Get()) = shuffled[i];
    }
    put_shuffled.Poison();
  }
  
  MergeConfig merge_config;
  merge_config.arity = 3;
  merge_config.total_read_buffer = 50;
  merge_config.chain.entry_size = 8;
  merge_config.chain.block_size = 500;
  merge_config.chain.block_count = 6;
  merge_config.chain.queue_length = 3;
  scoped_fd sorted(boss.Merge(merge_config));
  uint64_t got;
  for (uint64_t i = 0; i < kSize; ++i) {
    ReadOrThrow(sorted.get(), &got, sizeof(uint64_t));
    BOOST_CHECK_EQUAL(i, got);
  }
  BOOST_CHECK_THROW(ReadOrThrow(sorted.get(), &got, sizeof(uint64_t)), EndOfFileException);
}

}}} // namespaces

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

  SortConfig merge_config;
  merge_config.temp_prefix = "sort_test_temp";
  merge_config.arity = 3;
  merge_config.total_read_buffer = 50;
  merge_config.lazy_arity = 2;
  merge_config.lazy_total_read_buffer = 10;
  merge_config.chain.entry_size = 8;
  merge_config.chain.block_size = 500;
  merge_config.chain.block_count = 6;
  merge_config.chain.queue_length = 3;

  Sort<CompareUInt64> sorter(merge_config, CompareUInt64());
  {
    Chain chain(config);
    Stream put_shuffled(chain.Add());
    chain >> sorter.Unsorted();
    for (uint64_t i = 0; i < kSize; ++i, ++put_shuffled) {
      *static_cast<uint64_t*>(put_shuffled.Get()) = shuffled[i];
    }
    put_shuffled.Poison();
  }
  
  Stream sorted;
  Chain chain(config);
  chain >> sorter.Sorted() >> sorted >> kRecycle;
  for (uint64_t i = 0; i < kSize; ++i, ++sorted) {
    BOOST_CHECK_EQUAL(i, *static_cast<const uint64_t*>(sorted.Get()));
  }
  BOOST_CHECK(!sorted);
}

}}} // namespaces

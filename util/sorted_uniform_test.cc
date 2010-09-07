#include "util/sorted_uniform.hh"

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/scoped_array.hpp>
#include <boost/unordered_map.hpp>
#define BOOST_TEST_MODULE SortedUniformBoundTest
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <limits>
#include <vector>

namespace util {
namespace {

template <class Key, class Value> void Check(const SortedUniformMap<Key, Value> &map, const boost::unordered_map<Key, Value> &reference, const Key &key) {
  typename boost::unordered_map<Key, Value>::const_iterator ref = reference.find(key);
  if (ref == reference.end()) {
    const Value *val;
    BOOST_CHECK(!map.Find(key, val));
  } else {
    // g++ can't tell that require will crash and burn.
    const Value *val = NULL;
    BOOST_REQUIRE(map.Find(key, val));
    BOOST_CHECK_EQUAL(ref->second, *val);
  }
}

/*BOOST_AUTO_TEST_CASE(empty) {
  uint64_t foo;
  Check<uint64_t>(&foo, &foo, 1);
}

BOOST_AUTO_TEST_CASE(one) {
  uint64_t array[] = {1};
  Check<uint64_t>(&array[0], &array[1], 1);
  Check<uint64_t>(&array[0], &array[1], 0);
}*/

template <class Key> void RandomTest(Key upper, size_t entries, size_t queries) {
  typedef unsigned char Value;
  typedef SortedUniformMap<Key, unsigned char> Map;
  boost::scoped_array<char> buffer(new char[Map::Size(typename Map::Init(), entries)]);
  Map map(typename Map::Init(), buffer.get(), entries);
  boost::mt19937 rng;
  boost::uniform_int<Key> range_key(0, upper);
  boost::uniform_int<Value> range_value(0, 255);
  boost::variate_generator<boost::mt19937&, boost::uniform_int<Key> > gen_key(rng, range_key);
  boost::variate_generator<boost::mt19937&, boost::uniform_int<unsigned char> > gen_value(rng, range_value);

  boost::unordered_map<Key, unsigned char> reference;
  for (size_t i = 0; i < entries; ++i) {
    Key key = gen_key();
    unsigned char value = gen_value();
    if (reference.insert(std::make_pair(key, value)).second) {
      map.Insert(key, value);
    }
  }
  map.FinishedInserting();

  // Random queries.  
  for (size_t i = 0; i < queries; ++i) {
    const Key key = gen_key();
    Check<Key, Value>(map, reference, key);
  }

  typename boost::unordered_map<Key, unsigned char>::const_iterator it = reference.begin();
  for (size_t i = 0; (i < queries) && (it != reference.end()); ++i, ++it) {
    Check<Key,Value>(map, reference, it->second);
  }
}

BOOST_AUTO_TEST_CASE(sparse_random) {
  RandomTest<uint64_t>(std::numeric_limits<uint64_t>::max(), 100000, 2000);
}

BOOST_AUTO_TEST_CASE(tiny_dense_random) {
  RandomTest<uint8_t>(11, 50, 200);
}

BOOST_AUTO_TEST_CASE(small_dense_random) {
  RandomTest<uint8_t>(100, 100, 200);
}

BOOST_AUTO_TEST_CASE(small_sparse_random) {
  RandomTest<uint8_t>(200, 15, 200);
}

BOOST_AUTO_TEST_CASE(medium_sparse_random) {
  RandomTest<uint16_t>(32000, 1000, 2000);
}

} // namespace
} // namespace util

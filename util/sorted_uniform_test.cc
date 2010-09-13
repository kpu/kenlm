#include "util/sorted_uniform.hh"

#include "util/key_value_packing.hh"

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/scoped_array.hpp>
#include <boost/unordered_map.hpp>
#define BOOST_TEST_MODULE SortedUniformTest
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <limits>
#include <vector>

namespace util {
namespace {

template <class Map, class Key, class Value> void Check(const Map &map, const boost::unordered_map<Key, Value> &reference, const Key key) {
  typename boost::unordered_map<Key, Value>::const_iterator ref = reference.find(key);
  typename Map::ConstIterator i = typename Map::ConstIterator();
  if (ref == reference.end()) {
    BOOST_CHECK(!map.Find(key, i));
  } else {
    // g++ can't tell that require will crash and burn.
    BOOST_REQUIRE(map.Find(key, i));
    BOOST_CHECK_EQUAL(ref->second, i->GetValue());
  }
}

typedef SortedUniformMap<AlignedPacking<uint64_t, uint32_t> > TestMap;

BOOST_AUTO_TEST_CASE(empty) {
  char buf[TestMap::Size(0)];
  TestMap map(buf, TestMap::Size(0));
  map.FinishedInserting();
  TestMap::ConstIterator i;
  BOOST_CHECK(!map.Find(42, i));
}

BOOST_AUTO_TEST_CASE(one) {
  char buf[TestMap::Size(1)];
  TestMap map(buf, sizeof(buf));
  Entry<uint64_t, uint32_t> e;
  e.Set(42,2);
  map.Insert(e);
  map.FinishedInserting();
  TestMap::ConstIterator i = TestMap::ConstIterator();
  BOOST_REQUIRE(map.Find(42, i));
  BOOST_CHECK(i == map.begin());
  BOOST_CHECK(!map.Find(43, i));
  BOOST_CHECK(!map.Find(41, i));
}

template <class Key> void RandomTest(Key upper, size_t entries, size_t queries) {
  typedef unsigned char Value;
  typedef SortedUniformMap<AlignedPacking<Key, unsigned char> > Map;
  boost::scoped_array<char> buffer(new char[Map::Size(entries)]);
  Map map(buffer.get(), entries);
  boost::mt19937 rng;
  boost::uniform_int<Key> range_key(0, upper);
  boost::uniform_int<Value> range_value(0, 255);
  boost::variate_generator<boost::mt19937&, boost::uniform_int<Key> > gen_key(rng, range_key);
  boost::variate_generator<boost::mt19937&, boost::uniform_int<unsigned char> > gen_value(rng, range_value);

  boost::unordered_map<Key, unsigned char> reference;
  Entry<Key, unsigned char> ent;
  for (size_t i = 0; i < entries; ++i) {
    Key key = gen_key();
    unsigned char value = gen_value();
    if (reference.insert(std::make_pair(key, value)).second) {
      ent.Set(key, value);
      map.Insert(Entry<Key, unsigned char>(ent));
    }
  }
  map.FinishedInserting();

  // Random queries.  
  for (size_t i = 0; i < queries; ++i) {
    const Key key = gen_key();
    Check<Map, Key, unsigned char>(map, reference, key);
  }

  typename boost::unordered_map<Key, unsigned char>::const_iterator it = reference.begin();
  for (size_t i = 0; (i < queries) && (it != reference.end()); ++i, ++it) {
    Check<Map, Key, unsigned char>(map, reference, it->second);
  }
}

BOOST_AUTO_TEST_CASE(basic) {
  RandomTest<uint8_t>(11, 10, 200);
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

BOOST_AUTO_TEST_CASE(sparse_random) {
  RandomTest<uint64_t>(std::numeric_limits<uint64_t>::max(), 100000, 2000);
}

} // namespace
} // namespace util

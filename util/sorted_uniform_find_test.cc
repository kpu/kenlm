#include "util/sorted_uniform_find.hh"

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/unordered_set.hpp>
#define BOOST_TEST_MODULE SortedUniformFindTest
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <limits>
#include <vector>

namespace util {
namespace {

BOOST_AUTO_TEST_CASE(empty) {
	uint64_t array[] = {};
	BOOST_CHECK_EQUAL((const uint64_t*)0, (SortedUniformFind<uint64_t>(array, array - 1, 0)));
}

BOOST_AUTO_TEST_CASE(one) {
	uint64_t array[] = {1};
	BOOST_CHECK_EQUAL(0, (SortedUniformFind<uint64_t>(array, array, 1) - array));
	BOOST_CHECK_EQUAL((const uint64_t*)0, (SortedUniformFind<uint64_t>(array, array, 0)));
}

template <class Value> void RandomTest(Value upper, size_t entries, size_t queries) {
	boost::mt19937 rng;
	boost::uniform_int<Value> range(0, upper);
	boost::variate_generator<boost::mt19937&, boost::uniform_int<Value> > gen(rng, range);
	std::vector<Value> values;
	values.reserve(entries);
	for (size_t i = 0; i < entries; ++i) {
		values.push_back(gen());
	}
	std::sort(values.begin(), values.end());
	boost::unordered_set<Value> values_set;
	for (typename std::vector<Value>::const_iterator i = values.begin(); i != values.end(); ++i) {
		values_set.insert(*i);
	}
	const Value *left = &*values.begin();
	const Value *right = &*values.end() - 1;

	// Random queries.  
	for (size_t i = 0; i < queries; ++i) {
		const Value key = gen();
		const Value *ret = SortedUniformFind(left, right, key);
		if (values_set.find(key) != values_set.end()) {
			BOOST_REQUIRE(ret);
			BOOST_CHECK_EQUAL(*ret, key);
		} else {
			BOOST_CHECK_EQUAL((const Value *)0, ret);
		}
	}

	// Inside queries.
	boost::uniform_int<size_t> inside_range(0, entries - 1);
	boost::variate_generator<boost::mt19937&, boost::uniform_int<size_t> > inside_gen(rng, inside_range);
	for (size_t i = 0; i < queries; ++i) {
		const size_t off = inside_gen();
		const Value *ret = SortedUniformFind(left, right, values[off]);
		BOOST_REQUIRE(ret);
		BOOST_CHECK_EQUAL(*(left + off), *ret);
	}

}

BOOST_AUTO_TEST_CASE(sparse_random) {
  RandomTest(std::numeric_limits<uint64_t>::max(), 100000, 2000);
}

BOOST_AUTO_TEST_CASE(tiny_dense_random) {
	RandomTest<uint8_t>(11, 50, 200);
}

BOOST_AUTO_TEST_CASE(small_dense_random) {
	RandomTest<uint8_t>(100, 100, 200);
}

BOOST_AUTO_TEST_CASE(dense_random) {
  RandomTest<uint16_t>(10000, 100000, 2000);
}

} // namespace
} // namespace util

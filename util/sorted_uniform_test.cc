#include "util/sorted_uniform.hh"

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/unordered_set.hpp>
#define BOOST_TEST_MODULE SortedUniformBoundTest
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <limits>
#include <vector>

namespace util {
namespace {

template <class Value> void Check(const Value *begin, const Value *end, Value key) {
	std::pair<const Value *, const Value *> range(std::equal_range(begin, end, key));
	const Value *reported = SortedUniformBound<Value>(begin, end, key);
	if (reported < range.first) BOOST_ERROR("Went too low");
	if (reported > range.second) BOOST_ERROR("Went too high");
}

BOOST_AUTO_TEST_CASE(empty) {
	uint64_t foo;
	Check<uint64_t>(&foo, &foo, 1);
}

BOOST_AUTO_TEST_CASE(one) {
	uint64_t array[] = {1};
	Check<uint64_t>(&array[0], &array[1], 1);
	Check<uint64_t>(&array[0], &array[1], 0);
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

	const Value *begin = &*values.begin();
	const Value *end = &*values.end();
/*	for (const Value *i = begin; i != end; ++i) {
		std::cout << static_cast<unsigned int>(*i) << ' ';
	}
	std::cout << std::endl;*/

	// Random queries.  
	for (size_t i = 0; i < queries; ++i) {
		const Value key = gen();
		Check<Value>(begin, end, key);
	}

	// Inside queries.
	boost::uniform_int<size_t> inside_range(0, entries - 1);
	boost::variate_generator<boost::mt19937&, boost::uniform_int<size_t> > inside_gen(rng, inside_range);
	for (size_t i = 0; i < queries; ++i) {
		const size_t off = inside_gen();
		Check<Value>(begin, end, values[off]);
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

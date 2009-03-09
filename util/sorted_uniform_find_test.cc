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
	BOOST_CHECK_EQUAL((const uint64_t*)0, (SortedUniformFind(array, array - 1, 0)));
}

BOOST_AUTO_TEST_CASE(one) {
	uint64_t array[] = {1};
	BOOST_CHECK_EQUAL(0, (SortedUniformFind(array, array, 1) - array));
	BOOST_CHECK_EQUAL((const uint64_t*)0, (SortedUniformFind(array, array, 0)));
}

void RandomTest(uint64_t upper, uint64_t entries, uint64_t queries) {
	boost::mt19937 rng;
	boost::uniform_int<uint64_t> range(0, upper);
	boost::variate_generator<boost::mt19937&, boost::uniform_int<uint64_t> > gen(rng, range);
	std::vector<uint64_t> values;
	values.reserve(entries);
	for (uint64_t i = 0; i < entries; ++i) {
		values.push_back(gen());
	}
	std::sort(values.begin(), values.end());
	boost::unordered_set<uint64_t> values_set;
	for (std::vector<uint64_t>::const_iterator i = values.begin(); i != values.end(); ++i) {
		values_set.insert(*i);
	}
	const uint64_t *left = &*values.begin();
	const uint64_t *right = &*values.end() - 1;

	// Random queries.  
	for (uint64_t i = 0; i < queries; ++i) {
		const uint64_t key = gen();
		const uint64_t *ret = SortedUniformFind(left, right, key);
		if (values_set.find(key) != values_set.end()) {
			if (!ret) {
				std::cerr << "Did not find " << key << " on iteration " << i << std::endl;
				for (std::vector<uint64_t>::const_iterator j = values.begin(); j != values.end(); ++j) {
					std::cerr << *j << ' ';
				}
				std::cerr << std::endl;
			}
			BOOST_REQUIRE(ret);
			BOOST_CHECK_EQUAL(*ret, key);
		} else {
			BOOST_CHECK_EQUAL((const uint64_t *)0, ret);
		}
	}

	// Inside queries.
	boost::uniform_int<uint64_t> inside_range(0, entries - 1);
	boost::variate_generator<boost::mt19937&, boost::uniform_int<uint64_t> > inside_gen(rng, inside_range);
	for (uint64_t i = 0; i < queries; ++i) {
		const uint64_t off = inside_gen();
		const uint64_t *ret = SortedUniformFind(left, right, values[off]);
		if (!ret) {
			std::cerr << "Did not find " << values[off] << " at " << off << " on iteration " << i << std::endl;
			for (std::vector<uint64_t>::const_iterator j = values.begin(); j != values.end(); ++j) {
				std::cerr << *j << ' ';
			}
			std::cerr << std::endl;
		}
		BOOST_REQUIRE(ret);
		BOOST_CHECK_EQUAL(*(left + off), *ret);
	}

}

BOOST_AUTO_TEST_CASE(sparse_random) {
  RandomTest(std::numeric_limits<uint64_t>::max(), 100000, 2000);
}

BOOST_AUTO_TEST_CASE(small_dense_random) {
	RandomTest(11, 11, 200);
}

BOOST_AUTO_TEST_CASE(small_dense_random) {
	RandomTest(100, 100, 200);
}

BOOST_AUTO_TEST_CASE(dense_random) {
  RandomTest(10000, 100000, 2000);
}

} // namespace
} // namespace util

#include "util/probing_hash_table.hh"

#define BOOST_TEST_MODULE ProbingHashTableTest
#include <boost/test/unit_test.hpp>

namespace util {
namespace {

BOOST_AUTO_TEST_CASE(simple) {
	uint8_t mem[10];
	memset(mem, 0, sizeof(mem));
	ProbingHashTable<uint8_t> table(mem, 10, 0);
	BOOST_CHECK_EQUAL((uint8_t*)NULL, table.Find(2));
	BOOST_CHECK_EQUAL((uint8_t)2, *table.Insert(2).first);
	BOOST_REQUIRE(table.Find(2));
	BOOST_CHECK_EQUAL((uint8_t)2, *table.Find(2));
}

} // namespace
} // namespace util

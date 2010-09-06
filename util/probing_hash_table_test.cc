#include "util/probing_hash_table.hh"

#define BOOST_TEST_MODULE ProbingHashTableTest
#include <boost/test/unit_test.hpp>
#include <boost/functional/hash.hpp>

namespace util {
namespace {

BOOST_AUTO_TEST_CASE(simple) {
	char mem[10];
	memset(mem, 0, sizeof(mem));
	ProbingHashTable<char, boost::hash<char> > table(mem, 10, 0);
	BOOST_CHECK_EQUAL((char*)NULL, table.Find(2));
	BOOST_CHECK_EQUAL((char)2, *table.Insert(2).first);
	BOOST_REQUIRE(table.Find(2));
	BOOST_CHECK_EQUAL((char)2, *table.Find(2));
}

} // namespace
} // namespace util

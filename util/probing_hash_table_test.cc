#include "util/probing_hash_table.hh"

#include "util/key_value_packing.hh"

#define BOOST_TEST_MODULE ProbingHashTableTest
#include <boost/test/unit_test.hpp>
#include <boost/functional/hash.hpp>

namespace util {
namespace {

typedef AlignedPacking<char, uint64_t> Packing;
typedef ProbingHashTable<Packing, boost::hash<char> > Table;

BOOST_AUTO_TEST_CASE(simple) {
  char mem[Table::Size(10, 1.2)];
  memset(mem, 0, sizeof(mem));

  Table table(mem, sizeof(mem));
  Packing::ConstIterator i = Packing::ConstIterator();
  BOOST_CHECK(!table.Find(2, i));
  table.Insert(Packing::Make(3, 328920));
  BOOST_REQUIRE(table.Find(3, i));
  BOOST_CHECK_EQUAL(3, i->GetKey());
  BOOST_CHECK_EQUAL(static_cast<uint64_t>(328920), i->GetValue());
  BOOST_CHECK(!table.Find(2, i));
}

} // namespace
} // namespace util

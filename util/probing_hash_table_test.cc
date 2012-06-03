#include "util/probing_hash_table.hh"

#include <stdint.h>

#define BOOST_TEST_MODULE ProbingHashTableTest
#include <boost/test/unit_test.hpp>
#include <boost/functional/hash.hpp>

namespace util {
namespace {

struct Entry {
  unsigned char key;
  typedef unsigned char Key;

  unsigned char GetKey() const {
    return key;
  }

  uint64_t GetValue() const {
    return value;
  }

  uint64_t value;
};

typedef ProbingHashTable<Entry, boost::hash<unsigned char> > Table;

BOOST_AUTO_TEST_CASE(simple) {
  char mem[Table::Size(10, 1.2)];
  memset(mem, 0, sizeof(mem));

  Table table(mem, sizeof(mem));
  const Entry *i = NULL;
  BOOST_CHECK(!table.Find(2, i));
  Entry to_ins;
  to_ins.key = 3;
  to_ins.value = 328920;
  table.Insert(to_ins);
  BOOST_REQUIRE(table.Find(3, i));
  BOOST_CHECK_EQUAL(3, i->GetKey());
  BOOST_CHECK_EQUAL(static_cast<uint64_t>(328920), i->GetValue());
  BOOST_CHECK(!table.Find(2, i));
}

} // namespace
} // namespace util

#include "util/probing_hash_table.hh"
#include "util/scoped.hh"
#include "util/usage.hh"

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include <iostream>

namespace util {
namespace {

struct Entry {
  typedef uint64_t Key;
  Key key;
  Key GetKey() const { return key; }
};

typedef util::ProbingHashTable<Entry, util::IdentityHash> Table;

void Test(uint64_t entries, uint64_t lookups, float multiplier = 1.5) {
  std::size_t size = Table::Size(entries, multiplier);
  scoped_malloc backing(util::CallocOrThrow(size));
  Table table(backing.get(), size);
  boost::random::mt19937 gen;
  boost::random::uniform_int_distribution<> dist(std::numeric_limits<uint64_t>::min(), std::numeric_limits<uint64_t>::max());
  double start = UserTime();
  for (uint64_t i = 0; i < entries; ++i) {
    Entry entry;
    entry.key = dist(gen);
    table.Insert(entry);
  }
  double inserted = UserTime();
  bool meaningless = true;
  for (uint64_t i = 0; i < lookups; ++i) {
    Table::ConstIterator it;
    meaningless ^= table.Find(dist(gen), it);
  }
  std::cout << meaningless << ' ' << entries << ' ' << multiplier << ' ' << (inserted - start) << ' ' << (UserTime() - inserted) / static_cast<double>(lookups) << std::endl;
}

} // namespace
} // namespace util

int main() {
  for (uint64_t i = 1; i <= 10000000ULL; i *= 10) {
    util::Test(i, 10000000);
  }
}

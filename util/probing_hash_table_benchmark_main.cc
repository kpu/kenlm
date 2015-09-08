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

template <class Mod> bool Test(uint64_t entries, uint64_t lookups = 20000000, float multiplier = 1.5) {
  typedef util::ProbingHashTable<Entry, util::IdentityHash, std::equal_to<Entry::Key>, Mod> Table;
  // Always round up to power of 2 for fair comparison.
  std::size_t size = Power2Mod::RoundBuckets(Table::Size(entries, multiplier) / sizeof(Entry)) * sizeof(Entry);
  scoped_malloc backing(util::CallocOrThrow(size));
  Table table(backing.get(), size);
  boost::random::mt19937 gen;
  boost::random::uniform_int_distribution<uint64_t> dist(std::numeric_limits<uint64_t>::min(), std::numeric_limits<uint64_t>::max());
  double start = UserTime();
  for (uint64_t i = 0; i < entries; ++i) {
    Entry entry;
    entry.key = dist(gen);
    table.Insert(entry);
  }
  double inserted = UserTime();
  bool meaningless = true;
  for (uint64_t i = 0; i < lookups; ++i) {
    typename Table::ConstIterator it;
    meaningless ^= table.Find(dist(gen), it);
  }
  std::cout << entries << ' ' << size << ' ' << (inserted - start) << ' ' << (UserTime() - inserted) / static_cast<double>(lookups) << '\n';
  return meaningless;
}

} // namespace
} // namespace util

int main() {
  bool meaningless = false;
  std::cout << "#Integer division\n";
  for (uint64_t i = 1; i <= 100000000ULL; i *= 10) {
    meaningless ^= util::Test<util::DivMod>(i);
  }
  std::cout << "#Masking\n";
  for (uint64_t i = 1; i <= 100000000ULL; i *= 10) {
    meaningless ^= util::Test<util::Power2Mod>(i);
  }
  std::cerr << "Meaningless: " << meaningless << '\n';
}

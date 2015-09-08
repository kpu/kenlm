#include "util/probing_hash_table.hh"
#include "util/scoped.hh"
#include "util/usage.hh"

#include <boost/random/mersenne_twister.hpp>
#include <boost/version.hpp>
#if BOOST_VERSION >= 104700
#include <boost/random/uniform_int_distribution.hpp>
#define UTIL_TWISTER boost::random::mt19937
#define UTIL_INT_DIST boost::random::uniform_int_distribution
#else
#include <boost/random/uniform_int.hpp>
#define UTIL_TWISTER boost::mt19937
#define UTIL_INT_DIST boost::uniform_int
#endif

#include <iostream>

namespace util {
namespace {

struct Entry {
  typedef uint64_t Key;
  Key key;
  Key GetKey() const { return key; }
};

template <class Mod> bool Test(uint64_t entries, const std::vector<uint64_t> &look, float multiplier = 1.5) {
  typedef util::ProbingHashTable<Entry, util::IdentityHash, std::equal_to<Entry::Key>, Mod> Table;
  // Always round up to power of 2 for fair comparison.
  std::size_t size = Power2Mod::RoundBuckets(Table::Size(entries, multiplier) / sizeof(Entry)) * sizeof(Entry);
  std::cerr << (static_cast<float>(size / sizeof(Entry)) / entries) << std::endl;
  scoped_malloc backing(util::CallocOrThrow(size));
  Table table(backing.get(), size);

  UTIL_TWISTER gen;
  UTIL_INT_DIST<uint64_t> dist(std::numeric_limits<uint64_t>::min(), std::numeric_limits<uint64_t>::max());
  double start = UserTime();
  for (uint64_t i = 0; i < entries; ++i) {
    Entry entry;
    entry.key = dist(gen);
    table.Insert(entry);
  }
  double inserted = UserTime() - start;
  bool meaningless = true;
  double before_lookup = UserTime();
  for (uint64_t i = 0; i < look.size(); ++i) {
    typename Table::ConstIterator it;
    meaningless ^= table.Find(look[i], it);
  }
  std::cout << entries << ' ' << size << ' ' << inserted / static_cast<double>(entries) << ' ' << (UserTime() - before_lookup) / static_cast<double>(look.size()) << '\n';
  return meaningless;
}

template <class Mod> bool TestRun(uint64_t lookups = 20000000, float multiplier = 1.5) {
  std::vector<uint64_t> look;
  look.reserve(lookups);
  UTIL_TWISTER gen;
  UTIL_INT_DIST<uint64_t> dist(std::numeric_limits<uint64_t>::min(), std::numeric_limits<uint64_t>::max());
  for (uint64_t i = 0; i < lookups; ++i) {
    look.push_back(dist(gen));
  }

  bool meaningless = true;
  for (uint64_t i = 4; i <= 100000000ULL; i *= 4) {
    meaningless ^= util::Test<Mod>(i / multiplier, look, multiplier);
  }
  return meaningless;
}

} // namespace
} // namespace util

int main() {
  bool meaningless = false;
  std::cout << "#Integer division\n";
  meaningless ^= util::TestRun<util::DivMod>();
  std::cout << "#Masking\n";
  meaningless ^= util::TestRun<util::Power2Mod>();
  std::cerr << "Meaningless: " << meaningless << '\n';
}

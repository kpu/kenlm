#include "util/file.hh"
#include "util/probing_hash_table.hh"
#include "util/scoped.hh"
#include "util/usage.hh"

#include <iostream>

namespace util {
namespace {

struct Entry {
  typedef uint64_t Key;
  Key key;
  Key GetKey() const { return key; }
};

// I don't care if this doesn't run on Windows.  Empirically /dev/urandom was faster than boost::random's Mersenne Twister.
class URandom {
  public:
    URandom() :
      it_(buf_ + 1024), end_(buf_ + 1024),
      file_(util::OpenReadOrThrow("/dev/urandom")) {}

    uint64_t Get() {
      if (it_ == end_) {
        it_ = buf_;
        util::ReadOrThrow(file_.get(), buf_, sizeof(buf_));
        it_ = buf_;
      }
      return *it_++;
    }

    void Batch(uint64_t *begin, uint64_t *end) {
      util::ReadOrThrow(file_.get(), begin, (end - begin) * sizeof(uint64_t));
    }

  private:
    uint64_t buf_[1024];
    uint64_t *it_, *end_;

    util::scoped_fd file_;
};

template <class Mod> bool Test(URandom &rn, uint64_t entries, const uint64_t *const queries_begin, const uint64_t *const queries_end, float multiplier = 1.5) {
  typedef util::ProbingHashTable<Entry, util::IdentityHash, std::equal_to<Entry::Key>, Mod> Table;
  // Always round up to power of 2 for fair comparison.
  std::size_t size = Power2Mod::RoundBuckets(Table::Size(entries, multiplier) / sizeof(Entry)) * sizeof(Entry);
  scoped_malloc backing(util::CallocOrThrow(size));
  Table table(backing.get(), size);

  double start = UserTime();
  for (uint64_t i = 0; i < entries; ++i) {
    Entry entry;
    entry.key = rn.Get();
    table.Insert(entry);
  }
  double inserted = UserTime() - start;
  bool meaningless = true;
  double before_lookup = UserTime();
  for (const uint64_t *i = queries_begin; i != queries_end; ++i) {
    typename Table::ConstIterator it;
    meaningless ^= table.Find(*i, it);
  }
  std::cout << entries << ' ' << size << ' ' << (inserted / static_cast<double>(entries)) << ' ' << (UserTime() - before_lookup) / static_cast<double>(queries_end - queries_begin) << '\n';
  return meaningless;
}

template <class Mod> bool TestRun(uint64_t lookups = 20000000, float multiplier = 1.5) {
  URandom rn;
  util::scoped_malloc queries(util::CallocOrThrow(lookups * sizeof(uint64_t)));
  rn.Batch(static_cast<uint64_t*>(queries.get()), static_cast<uint64_t*>(queries.get()) + lookups);
  
  bool meaningless = true;
  for (uint64_t i = 4; i <= 10000000000ULL; i *= 4) {
    meaningless ^= util::Test<Mod>(rn, i / multiplier, static_cast<const uint64_t*>(queries.get()), static_cast<const uint64_t*>(queries.get()) + lookups, multiplier);
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

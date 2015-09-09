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

struct PrefetchEntry {
  uint64_t key;
  const Entry *pointer;
};

const std::size_t kPrefetchSize = 4;
template <class Table> class PrefetchQueue {
  public:
    explicit PrefetchQueue(Table &table) : table_(table), cur_(0), twiddle_(false) {
      for (PrefetchEntry *i = entries_; i != entries_ + kPrefetchSize; ++i)
        i->pointer = NULL;
    }

    void Add(uint64_t key) {
      if (Cur().pointer) {
        twiddle_ ^= table_.FindFromIdeal(Cur().key, Cur().pointer);
      }
      Cur().key = key;
      Cur().pointer = table_.Ideal(key);
      __builtin_prefetch(Cur().pointer, 0, 0);
      Next();
    }

    bool Drain() {
      if (Cur().pointer) {
        for (PrefetchEntry *i = &Cur(); i < entries_ + kPrefetchSize; ++i) {
          twiddle_ ^= table_.FindFromIdeal(i->key, i->pointer);
        }
      }
      for (PrefetchEntry *i = entries_; i < &Cur(); ++i) {
        twiddle_ ^= table_.FindFromIdeal(i->key, i->pointer);
      }
      return twiddle_;
    }

  private:
    PrefetchEntry &Cur() { return entries_[cur_]; }
    void Next() {
      ++cur_;
      cur_ = cur_ % kPrefetchSize;
    }

    Table &table_;
    PrefetchEntry entries_[kPrefetchSize];
    std::size_t cur_;

    bool twiddle_;

    PrefetchQueue(const PrefetchQueue&);
    void operator=(const PrefetchQueue&);
};

/*template <class Table> class Immediate {
  public:

  private:
    Table &table_;
};*/

std::size_t Size(uint64_t entries, float multiplier = 1.5) {
  typedef util::ProbingHashTable<Entry, util::IdentityHash, std::equal_to<Entry::Key>, Power2Mod> Table;
  // Always round up to power of 2 for fair comparison.
  return Power2Mod::RoundBuckets(Table::Size(entries, multiplier) / sizeof(Entry)) * sizeof(Entry);
}

template <class Mod> bool Test(URandom &rn, uint64_t entries, const uint64_t *const queries_begin, const uint64_t *const queries_end, float multiplier = 1.5) {
  typedef util::ProbingHashTable<Entry, util::IdentityHash, std::equal_to<Entry::Key>, Mod> Table;
  std::size_t size = Size(entries, multiplier);
  scoped_malloc backing(util::CallocOrThrow(size));
  Table table(backing.get(), size);

  double start = UserTime();
  for (uint64_t i = 0; i < entries; ++i) {
    Entry entry;
    entry.key = rn.Get();
    table.Insert(entry);
  }
  double inserted = UserTime() - start;
  double before_lookup = UserTime();
  PrefetchQueue<Table> queue(table);
  for (const uint64_t *i = queries_begin; i != queries_end; ++i) {
    queue.Add(*i);
/*    typename Table::ConstIterator it;
    meaningless ^= table.Find(*i, it);*/
  }
  bool meaningless = queue.Drain();
  std::cout << entries << ' ' << size << ' ' << (inserted / static_cast<double>(entries)) << ' ' << (UserTime() - before_lookup) / static_cast<double>(queries_end - queries_begin) << '\n';
  return meaningless;
}

template <class Mod> bool TestRun(uint64_t lookups = 20000000, float multiplier = 1.5) {
  URandom rn;
  util::scoped_malloc queries(util::CallocOrThrow(lookups * sizeof(uint64_t)));
  rn.Batch(static_cast<uint64_t*>(queries.get()), static_cast<uint64_t*>(queries.get()) + lookups);
  uint64_t physical_mem_limit = util::GuessPhysicalMemory() / 2;
  bool meaningless = true;
  for (uint64_t i = 4; Size(i / multiplier) < physical_mem_limit; i *= 4) {
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

#include "util/file.hh"
#include "util/probing_hash_table.hh"
#include "util/mmap.hh"
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

template <class TableT, unsigned PrefetchSize> class PrefetchQueue {
  public:
    typedef TableT Table;

    explicit PrefetchQueue(Table &table) : table_(table), cur_(0), twiddle_(false) {
      for (PrefetchEntry *i = entries_; i != entries_ + PrefetchSize; ++i)
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
        for (PrefetchEntry *i = &Cur(); i < entries_ + PrefetchSize; ++i) {
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
      cur_ = cur_ % PrefetchSize;
    }

    Table &table_;
    PrefetchEntry entries_[PrefetchSize];
    std::size_t cur_;

    bool twiddle_;

    PrefetchQueue(const PrefetchQueue&);
    void operator=(const PrefetchQueue&);
};

template <class TableT> class Immediate {
  public:
    typedef TableT Table;

    explicit Immediate(Table &table) : table_(table), twiddle_(false) {}

    void Add(uint64_t key) {
      typename Table::ConstIterator it;
      twiddle_ ^= table_.Find(key, it);
    }

    bool Drain() const { return twiddle_; }

  private:
    Table &table_;
    bool twiddle_;
};

std::size_t Size(uint64_t entries, float multiplier = 1.5) {
  typedef util::ProbingHashTable<Entry, util::IdentityHash, std::equal_to<Entry::Key>, Power2Mod> Table;
  // Always round up to power of 2 for fair comparison.
  return Power2Mod::RoundBuckets(Table::Size(entries, multiplier) / sizeof(Entry)) * sizeof(Entry);
}

template <class Queue> bool Test(URandom &rn, uint64_t entries, const uint64_t *const queries_begin, const uint64_t *const queries_end, bool ordinary_malloc, float multiplier = 1.5) {
  std::size_t size = Size(entries, multiplier);
  scoped_memory backing;
  if (ordinary_malloc) {
    backing.reset(util::CallocOrThrow(size), size, scoped_memory::MALLOC_ALLOCATED);
  } else {
    util::HugeMalloc(size, true, backing);
  }
  typename Queue::Table table(backing.get(), size);

  double start = CPUTime();
  for (uint64_t i = 0; i < entries; ++i) {
    Entry entry;
    entry.key = rn.Get();
    table.Insert(entry);
  }
  double inserted = CPUTime() - start;
  double before_lookup = CPUTime();
  Queue queue(table);
  for (const uint64_t *i = queries_begin; i != queries_end; ++i) {
    queue.Add(*i);
  }
  bool meaningless = queue.Drain();
  std::cout << ' ' << (inserted / static_cast<double>(entries)) << ' ' << (CPUTime() - before_lookup) / static_cast<double>(queries_end - queries_begin) << std::flush;
  return meaningless;
}

bool TestRun(uint64_t lookups = 20000000, float multiplier = 1.5) {
  URandom rn;
  util::scoped_memory queries;
  HugeMalloc(lookups * sizeof(uint64_t), true, queries);
  rn.Batch(static_cast<uint64_t*>(queries.get()), static_cast<uint64_t*>(queries.get()) + lookups);
  uint64_t physical_mem_limit = util::GuessPhysicalMemory() / 2;
  bool meaningless = true;
  for (uint64_t i = 4; Size(i / multiplier) < physical_mem_limit; i *= 4) {
    std::cout << static_cast<std::size_t>(i / multiplier) << ' ' << Size(i / multiplier);
    typedef util::ProbingHashTable<Entry, util::IdentityHash, std::equal_to<Entry::Key>, Power2Mod> Table;
    typedef util::ProbingHashTable<Entry, util::IdentityHash, std::equal_to<Entry::Key>, DivMod> TableDiv;
    const uint64_t *const queries_begin = static_cast<const uint64_t*>(queries.get());
    meaningless ^= util::Test<Immediate<TableDiv> >(rn, i / multiplier, queries_begin, queries_begin + lookups, true, multiplier);
    meaningless ^= util::Test<Immediate<Table> >(rn, i / multiplier, queries_begin, queries_begin + lookups, true, multiplier);
    meaningless ^= util::Test<PrefetchQueue<Table, 4> >(rn, i / multiplier, queries_begin, queries_begin + lookups, true, multiplier);
    meaningless ^= util::Test<Immediate<Table> >(rn, i / multiplier, queries_begin, queries_begin + lookups, false, multiplier);
    meaningless ^= util::Test<PrefetchQueue<Table, 2> >(rn, i / multiplier, queries_begin, queries_begin + lookups, false, multiplier);
    meaningless ^= util::Test<PrefetchQueue<Table, 4> >(rn, i / multiplier, queries_begin, queries_begin + lookups, false, multiplier);
    meaningless ^= util::Test<PrefetchQueue<Table, 8> >(rn, i / multiplier, queries_begin, queries_begin + lookups, false, multiplier);
    meaningless ^= util::Test<PrefetchQueue<Table, 16> >(rn, i / multiplier, queries_begin, queries_begin + lookups, false, multiplier);
    std::cout << std::endl;
  }
  return meaningless;
}

} // namespace
} // namespace util

int main() {
  bool meaningless = false;
  std::cout << "#CPU time\n";
  meaningless ^= util::TestRun();
  std::cerr << "Meaningless: " << meaningless << '\n';
}

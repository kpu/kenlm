#ifndef UTIL_PROBING_HASH_TABLE__
#define UTIL_PROBING_HASH_TABLE__

#include <algorithm>
#include <cstddef>
#include <functional>

#include <assert.h>

namespace util {

/* Non-standard hash table
 * Buckets must be set at the beginning and must be greater than maximum number
 * of elements, else an infinite loop happens.
 * Memory management and initialization is externalized to make it easier to
 * serialize these to disk and load them quickly.
 * Uses linear probing to find value.
 * Only insert and lookup operations.  
 */

template <class PackingT, class HashT, class EqualT = std::equal_to<typename PackingT::Key> > class ProbingHashTable {
  public:
    typedef PackingT Packing;
    typedef typename Packing::Key Key;
    typedef typename Packing::MutableIterator MutableIterator;
    typedef typename Packing::ConstIterator ConstIterator;

    typedef HashT Hash;
    typedef EqualT Equal;

    static std::size_t Size(float multiplier, std::size_t entries) {
      return std::max(entries + 1, static_cast<std::size_t>(multiplier * static_cast<float>(entries))) * Packing::kBytes;
    }

    // Must be assigned to later.  
    ProbingHashTable()
#ifndef NDEBUG
      : initialized_(false), entries_(0) 
#endif
    {}

    ProbingHashTable(void *start, std::size_t allocated, const Key &invalid = Key(), const Hash &hash_func = Hash(), const Equal &equal_func = Equal())
      : begin_(Packing::FromVoid(start)),
        buckets_(allocated / Packing::kBytes),
        end_(begin_ + (allocated / Packing::kBytes)),
        invalid_(invalid),
        hash_(hash_func),
        equal_(equal_func) 
#ifndef NDEBUG
        , initialized_(true),
        entries_(0) 
#endif
    {}

    template <class T> void Insert(const T &t) {
#ifndef NDEBUG
      assert(initialized_);
      assert(++entries_ < buckets_);
#endif
      for (MutableIterator i(begin_ + (hash_(t.GetKey()) % buckets_));;) {
        if (equal_(i->GetKey(), invalid_)) { *i = t; return; }
        if (++i == end_) { i = begin_; }
      }
    }

    void FinishedInserting() {}

    template <class Key> bool Find(const Key key, ConstIterator &out) const {
#ifndef NDEBUG
      assert(initialized_);
#endif
      for (ConstIterator i(begin_ + (hash_(key) % buckets_));;) {
        Key got(i->GetKey());
        if (equal_(got, key)) { out = i; return true; }
        if (equal_(got, invalid_)) { return false; }
        if (++i == end_) { i = begin_; }
      }    
    }

  private:
    MutableIterator begin_;
    std::size_t buckets_;
    MutableIterator end_;
    Key invalid_;
    Hash hash_;
    Equal equal_;
#ifndef NDEBUG
    bool initialized_;
    std::size_t entries_;
#endif
};

} // namespace util

#endif // UTIL_PROBING_HASH_TABLE__

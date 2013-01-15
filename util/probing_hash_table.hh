#ifndef UTIL_PROBING_HASH_TABLE__
#define UTIL_PROBING_HASH_TABLE__

#include "util/exception.hh"

#include <algorithm>
#include <cstddef>
#include <functional>

#include <assert.h>
#include <stdint.h>

namespace util {

/* Thrown when table grows too large */
class ProbingSizeException : public Exception {
  public:
    ProbingSizeException() throw() {}
    ~ProbingSizeException() throw() {}
};

// std::identity is an SGI extension :-(
struct IdentityHash {
  template <class T> T operator()(T arg) const { return arg; }
};

/* Non-standard hash table
 * Buckets must be set at the beginning and must be greater than maximum number
 * of elements, else it throws ProbingSizeException.
 * Memory management and initialization is externalized to make it easier to
 * serialize these to disk and load them quickly.
 * Uses linear probing to find value.
 * Only insert and lookup operations.  
 */

template <class EntryT, class HashT, class EqualT = std::equal_to<typename EntryT::Key> > class ProbingHashTable {
  public:
    typedef EntryT Entry;
    typedef typename Entry::Key Key;
    typedef const Entry *ConstIterator;
    typedef Entry *MutableIterator;
    typedef HashT Hash;
    typedef EqualT Equal;

  public:
    static uint64_t Size(uint64_t entries, float multiplier) {
      uint64_t buckets = std::max(entries + 1, static_cast<uint64_t>(multiplier * static_cast<float>(entries)));
      return buckets * sizeof(Entry);
    }

    // Must be assigned to later.  
    ProbingHashTable() : entries_(0)
#ifdef DEBUG
      , initialized_(false)
#endif
    {}

    ProbingHashTable(void *start, std::size_t allocated, const Key &invalid = Key(), const Hash &hash_func = Hash(), const Equal &equal_func = Equal())
      : begin_(reinterpret_cast<MutableIterator>(start)),
        buckets_(allocated / sizeof(Entry)),
        end_(begin_ + buckets_),
        invalid_(invalid),
        hash_(hash_func),
        equal_(equal_func),
        entries_(0)
#ifdef DEBUG
        , initialized_(true)
#endif
    {}

    template <class T> MutableIterator Insert(const T &t) {
#ifdef DEBUG
      assert(initialized_);
#endif
      UTIL_THROW_IF(++entries_ >= buckets_, ProbingSizeException, "Hash table with " << buckets_ << " buckets is full.");
      for (MutableIterator i(begin_ + (hash_(t.GetKey()) % buckets_));;) {
        if (equal_(i->GetKey(), invalid_)) { *i = t; return i; }
        if (++i == end_) { i = begin_; }
      }
    }

    // Return true if the value was found (and not inserted).  This is consistent with Find but the opposite if hash_map!
    template <class T> bool FindOrInsert(const T &t, MutableIterator &out) {
#ifdef DEBUG
      assert(initialized_);
#endif
      for (MutableIterator i(begin_ + (hash_(t.GetKey()) % buckets_));;) {
        Key got(i->GetKey());
        if (equal_(got, t.GetKey())) { out = i; return true; }
        if (equal_(got, invalid_)) {
          UTIL_THROW_IF(++entries_ >= buckets_, ProbingSizeException, "Hash table with " << buckets_ << " buckets is full.");
          *i = t;
          out = i;
          return false;
        }
        if (++i == end_) i = begin_;
      }   
    }

    void FinishedInserting() {}

    void LoadedBinary() {}

    // Don't change anything related to GetKey,  
    template <class Key> bool UnsafeMutableFind(const Key key, MutableIterator &out) {
#ifdef DEBUG
      assert(initialized_);
#endif
      for (MutableIterator i(begin_ + (hash_(key) % buckets_));;) {
        Key got(i->GetKey());
        if (equal_(got, key)) { out = i; return true; }
        if (equal_(got, invalid_)) return false;
        if (++i == end_) i = begin_;
      }   
    }

    template <class Key> bool Find(const Key key, ConstIterator &out) const {
#ifdef DEBUG
      assert(initialized_);
#endif
      for (ConstIterator i(begin_ + (hash_(key) % buckets_));;) {
        Key got(i->GetKey());
        if (equal_(got, key)) { out = i; return true; }
        if (equal_(got, invalid_)) return false;
        if (++i == end_) i = begin_;
      }    
    }

    void Clear(Entry invalid) {
      std::fill(begin_, end_, invalid);
      entries_ = 0;
    }

  private:
    MutableIterator begin_;
    std::size_t buckets_;
    MutableIterator end_;
    Key invalid_;
    Hash hash_;
    Equal equal_;
    std::size_t entries_;
#ifdef DEBUG
    bool initialized_;
#endif
};

} // namespace util

#endif // UTIL_PROBING_HASH_TABLE__

#ifndef UTIL_PROBING_HASH_TABLE__
#define UTIL_PROBING_HASH_TABLE__

#include "util/exception.hh"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <vector>

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
      return UncheckedInsert(t);
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

    // Like UnsafeMutableFind, but the key must be there.
    template <class Key> MutableIterator UnsafeMutableMustFind(const Key key) {
       for (MutableIterator i(begin_ + (hash_(key) % buckets_));;) {
        Key got(i->GetKey());
        if (equal_(got, key)) { return i; }
        assert(!equal_(got, invalid_));
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

    // Like Find but we're sure it must be there.
    template <class Key> ConstIterator MustFind(const Key key) const {
      for (ConstIterator i(begin_ + (hash_(key) % buckets_));;) {
        Key got(i->GetKey());
        if (equal_(got, key)) { return i; }
        assert(!equal_(got, invalid_));
        if (++i == end_) i = begin_;
      }
    }

    void Clear() {
      Entry invalid;
      invalid.SetKey(invalid_);
      std::fill(begin_, end_, invalid);
      entries_ = 0;
    }

    // Return number of entries assuming no serialization went on.
    std::size_t SizeNoSerialization() const {
      return entries_;
    }

    // Return memory size expected by Double.
    std::size_t DoubleTo() const {
      return buckets_ * 2 * sizeof(Entry);
    }

    // Inform the table that it has double the amount of memory.
    // Pass clear_new = false if you are sure the new memory is initialized
    // properly (to invalid_) i.e. by mremap.
    void Double(void *new_base, bool clear_new = true) {
      begin_ = static_cast<MutableIterator>(new_base);
      MutableIterator old_end = begin_ + buckets_;
      buckets_ *= 2;
      end_ = begin_ + buckets_;
      if (clear_new) {
        Entry invalid;
        invalid.SetKey(invalid_);
        std::fill(old_end, end_, invalid);
      }
      std::vector<Entry> rolled_over;
      // Move roll-over entries to a buffer because they might not roll over anymore.  This should be small.
      for (MutableIterator i = begin_; i != old_end && !equal_(i->GetKey(), invalid_); ++i) {
        rolled_over.push_back(*i);
        i->SetKey(invalid_);
      }
      /* Re-insert everything.  Entries might go backwards to take over a
       * recently opened gap, stay, move to new territory, or wrap around.   If
       * an entry wraps around, it might go to a pointer greater than i (which
       * can happen at the beginning) and it will be revisited to possibly fill
       * in a gap created later.
       */
      Entry temp;
      for (MutableIterator i = begin_; i != old_end; ++i) {
        if (!equal_(i->GetKey(), invalid_)) {
          temp = *i;
          i->SetKey(invalid_);
          UncheckedInsert(temp);
        }
      }
      // Put the roll-over entries back in.
      for (typename std::vector<Entry>::const_iterator i(rolled_over.begin()); i != rolled_over.end(); ++i) {
        UncheckedInsert(*i);
      }
    }

    // Mostly for tests, check consistency of every entry.
    void CheckConsistency() {
      MutableIterator last;
      for (last = end_ - 1; last >= begin_ && !equal_(last->GetKey(), invalid_); --last) {}
      UTIL_THROW_IF(last == begin_, ProbingSizeException, "Completely full");
      MutableIterator i;
      // Beginning can be wrap-arounds.
      for (i = begin_; !equal_(i->GetKey(), invalid_); ++i) {
        MutableIterator ideal = Ideal(*i);
        UTIL_THROW_IF(ideal > i && ideal <= last, Exception, "Inconsistency at position " << (i - begin_) << " should be at " << (ideal - begin_));
      }
      MutableIterator pre_gap = i;
      for (; i != end_; ++i) {
        if (equal_(i->GetKey(), invalid_)) {
          pre_gap = i;
          continue;
        }
        MutableIterator ideal = Ideal(*i);
        UTIL_THROW_IF(ideal > i || ideal <= pre_gap, Exception, "Inconsistency at position " << (i - begin_) << " with ideal " << (ideal - begin_));
      }
    }

  private:
    template <class T> MutableIterator Ideal(const T &t) {
      return begin_ + (hash_(t.GetKey()) % buckets_);
    }

    template <class T> MutableIterator UncheckedInsert(const T &t) {
      for (MutableIterator i(Ideal(t));;) {
        if (equal_(i->GetKey(), invalid_)) { *i = t; return i; }
        if (++i == end_) { i = begin_; }
      }
    }

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

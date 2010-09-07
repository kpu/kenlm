#ifndef UTIL_PROBING_HASH_TABLE__
#define UTIL_PROBING_HASH_TABLE__

#include <algorithm>
#include <cstddef>
#include <functional>
#include <utility>

namespace util {

/* Non-standard hash table
 * Buckets must be set at the beginning and must be greater than maximum number
 * of elements, else an infinite loop happens.
 * Memory management and initialization is externalized to make it easier to
 * serialize these to disk and load them quickly.
 * Uses linear probing to find value.
 * Only insert and lookup operations.  
 * Generic find operation.
 *
 */

template <class ValueT, class HashT, class EqualT = std::equal_to<ValueT>, class PointerT = const ValueT *> class ReadProbingHashTable {
  public:
    typedef ValueT Value;
    typedef HashT Hash;
    typedef EqualT Equal;

    ReadProbingHashTable() {}

    ReadProbingHashTable(
        PointerT start,
        std::size_t buckets,
        const Value &invalid,
        const Hash &hash_func = Hash(),
        const Equal &equal_func = Equal())
      : start_(start), end_(start + buckets), buckets_(buckets), invalid_(invalid), hash_(hash_func), equal_(equal_func) {}

    template <class Key> const Value *Find(const Key &key) const {
      const Value *it = start_ + (hash_(key) % buckets_);
      while (true) {
        if (equal_(*it, invalid_)) return NULL;
        if (equal_(*it, key)) return it;
        ++it;
        if (it == end_) it = start_;
      }
    }

  protected:
    PointerT start_, end_;
    std::size_t buckets_;
    Value invalid_;
    Hash hash_;
    Equal equal_;
};

template <class ValueT, class HashT, class EqualT = std::equal_to<ValueT> > class ProbingHashTable : public ReadProbingHashTable<ValueT, HashT, EqualT, ValueT *> {
  private:
    typedef ReadProbingHashTable<ValueT, HashT, EqualT, ValueT *> P;
    
  public:
    ProbingHashTable() {}

    // Memory should be initialized buckets copies of invalid.
    ProbingHashTable(
        typename P::Value *start,
        std::size_t buckets,
        const typename P::Value &invalid,
        const typename P::Hash &hash_func = typename P::Hash(),
        const typename P::Equal &equal_func = typename P::Equal())
      : P(start, buckets, invalid, hash_func, equal_func) {}

    std::pair<const typename P::Value *, bool> Insert(const typename P::Value &value) {
      typename P::Value *it = P::start_ + (P::hash_(value) % P::buckets_);
      while (!P::equal_(*it, P::invalid_)) {
        if (P::equal_(*it, value)) return std::pair<const typename P::Value*, bool>(it, false);
        ++it;
        if (it == P::end_) it = P::start_;
      }
      *it = value;
      return std::pair<const typename P::Value*, bool>(it, true);
    }
    
    const typename P::Value *InsertAlreadyUnique(const typename P::Value &value) {
      typename P::Value *it = P::start_ + (P::hash_(value) % P::buckets_);
      while (!P::equal_(*it, P::invalid_)) {
        ++it;
        if (it == P::end_) it = P::start_;
      }
      *it = value;
      return it;
    }
};

// Default configuration of the above: table from keys to values.  
template <class KeyT, class ValueT, class HashT, class EqualsT = std::equal_to<KeyT> > class ProbingMap {
  public:
    typedef KeyT Key;
    typedef ValueT Value;
    typedef HashT Hash;
    typedef EqualsT Equals;

    static std::size_t Size(float multiplier, std::size_t entries) {
      return std::max(entries + 1, static_cast<std::size_t>(multiplier * static_cast<float>(entries))) * sizeof(Entry);
    }

    ProbingMap() {}

    ProbingMap(float multiplier, char *start, std::size_t entries, const Hash &hasher = Hash(), const Equals &equals = Equals())
      : table_(
          reinterpret_cast<Entry*>(start),
          Size(multiplier, entries) / sizeof(Entry),
          Entry(),
          HashKeyOnly(hasher),
          EqualsKeyOnly(equals)) {}

    bool Find(const Key &key, const Value *&value) const {
      const Entry *e = table_.Find(key);
      if (!e) return false;
      value = &e->value;
      return true;
    }

    void Insert(const Key &key, const Value &value) {
      Entry e;
      e.key = key;
      e.value = value;
      table_.Insert(e);
    }

    void FinishedInserting() {}

  private:
    struct Entry {
      Key key;
      Value value;
    };
    class HashKeyOnly : public std::unary_function<const Entry &, std::size_t> {
      public:
        HashKeyOnly() {}
        explicit HashKeyOnly(const Hash &hasher) : hasher_(hasher) {}

        std::size_t operator()(const Entry &e) const { return hasher_(e.key); }
        std::size_t operator()(const Key value) const { return hasher_(value); }
      private:
        Hash hasher_;
    };
    struct EqualsKeyOnly : public std::binary_function<const Entry &, const Entry &, bool> {
      public:
        EqualsKeyOnly() {}
        explicit EqualsKeyOnly(const Equals &equals) : equals_(equals) {}

        bool operator()(const Entry &a, const Entry &b) const { return equals_(a.key, b.key); }
        bool operator()(const Entry &a, const Key k) const { return equals_(a.key, k); }

      private:
        Equals equals_;
    };

    ProbingHashTable<Entry, HashKeyOnly, EqualsKeyOnly> table_;
};

} // namespace util

#endif // UTIL_PROBING_HASH_TABLE__

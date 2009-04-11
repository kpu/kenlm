#ifndef UTIL_PROBING_HASH_TABLE__
#define UTIL_PROBING_HASH_TABLE__

#include <boost/functional/hash.hpp>

#include <functional>

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

template <class ValueT, class HashT = boost::hash<ValueT>, class EqualT = std::equal_to<ValueT>, class PointerT = const ValueT *> class ReadProbingHashTable {
	public:
		typedef ValueT Value;
		typedef HashT Hash;
		typedef EqualT Equal;

		ReadProbingHashTable(
				PointerT start,
				size_t buckets,
				const Value &invalid,
				const Hash &hash_func = Hash(),
				const Equal &equal_func = Equal())
			: start_(start), buckets_(buckets), invalid_(invalid), hash_(hash_func), equal_(equal_func) {}

		template <class Key> const Value *Find(const Key &key) const {
			const Value *it = start_ + (hash_(key) % buckets_);
			while (true) {
				if (equal_(*it, invalid_)) return NULL;
				if (equal_(*it, key)) return it;
				++it;
				if (it == start_ + buckets_) it = start_;
			}
		}

	protected:
		const PointerT start_;
		const size_t buckets_;
		const Value invalid_;
		const Hash hash_;
		const Equal equal_;
};

template <class ValueT, class HashT = boost::hash<ValueT>, class EqualT = std::equal_to<ValueT> > class ProbingHashTable : public ReadProbingHashTable<ValueT, HashT, EqualT, ValueT *> {
	private:
		typedef ReadProbingHashTable<ValueT, HashT, EqualT, ValueT *> P;
		
	public:
		// Memory should be initialized buckets copies of invalid.
		ProbingHashTable(
				typename P::Value *start,
				size_t buckets,
				const typename P::Value &invalid,
				const typename P::Hash &hash_func = typename P::Hash(),
				const typename P::Equal &equal_func = typename P::Equal())
			: P(start, buckets, invalid, hash_func, equal_func) {}

		std::pair<const typename P::Value *, bool> Insert(const typename P::Value &value) {
			typename P::Value *it = P::start_ + (P::hash_(value) % P::buckets_);
			while (!P::equal_(*it, P::invalid_)) {
				if (P::equal_(*it, value)) return std::pair<const typename P::Value*, bool>(it, false);
				++it;
				if (it == P::start_ + P::buckets_) it = P::start_;
			}
			*it = value;
			return std::pair<const typename P::Value*, bool>(it, true);

		}
		
		const typename P::Value *InsertAlreadyUnique(const typename P::Value &value) {
			typename P::Value *it = P::start_ + (P::hash_(value) % P::buckets_);
			while (!P::equal_(*it, P::invalid_)) {
				++it;
				if (it == P::start_ + P::buckets_) it = P::start_;
			}
			*it = value;
			return it;
		}
};

} // namespace util

#endif // UTIL_PROBING_HASH_TABLE__

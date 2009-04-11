#ifndef UTIL_SIMPLE_HASH_TABLE_H__
#define UTIL_SIMPLE_HASH_TABLE_H__

#include <assert.h>

namespace util {

/* Non-standard hash table optimized for language model storage.  The table is
 * statically sized using externally managed memory.  Collisions are handled by
 * moving to the next entry in the table, which means ValueT must have a
 * special "non-entry" value.  Only find and insert are supported, no erase.
 */
template <class ValueT, class HashT, class EqualsT, class PointerT = const ValueT*> class ReadSimpleHashTable {
	private:
		typedef PointerT Pointer;

	public:
		typedef ValueT Value;
		typedef HashT Hash;
		typedef EqualsT Equals;

	ReadSimpleHashTable(Pointer begin_p, Pointer end_p, const Value &not_value, const Hash &hash_func = Hash(), const Equals &equals_func = Equals())
			: begin_(begin_p), end_(end_p), not_value_(not_value), hash_(hash_func), equals_(equals_func) {}

		// Returns NULL if not found.
		template <class Key> const Value *Find(const Key &key) const {
			const Value *iter = Start(key);
			// Infinite loop if it can't find not_value_.  Could check for wrap around, but this shouldn't be so loaded.
			while (1) {
				if (equals_(*iter, key)) return iter;
				if (equals_(*iter, not_value_)) return NULL;
				if (++iter == end_) iter = begin_;
			}
		}

		const Value *table_begin() const { return begin_; }

		const Value *table_end() const { return end_; }

	private:
		template <class Key> const Value *Start(const Key &key) const {
			return &begin_[hash_(key) % (end_ - begin_)];
		}

		Pointer begin_;
		Pointer end_;

		const Value not_value_;
		const Hash hash_;
		const Equals equals_;
};

template <class ValueT, class HashT, class EqualsT> class SimpleHashTable : public ReadSimpleHashTable<ValueT, HashT, EqualsT, ValueT*> {
	private:
		typedef ReadSimpleHashTable<ValueT, HashT, EqualsT, ValueT*> P;

	public:
		SimpleHashTable(P::Pointer start, P::Pointer finish, const Value &not_value, const Hash &hash_func = Hash(), const Equals &equals_func = Equals())
			: P(start, finish, not_value, hash_func, equals_func) {}

		// Insert a value that is guaranteed by the caller to be unique and return a pointer to it.
		const P::Value *InsertAssumingUnique(const P::Value &value) const {
			assert(!P::equals_(value, P::not_value_));
			Value *iter = MutableStart(value);
			while (!P::equals_(*iter, P::not_value_)) {
				assert(!P::equals_(*iter, value));
				if (++iter == P::end_) iter = P::begin_;
			}
			*iter = value;
			return iter;
		}

	private:
		template <class Key> Value *MutableStart(const Key &key) {
			return &P::begin_[P::hash_(key) % (P::end_ - P::begin_)];
		}
};

} // namespace util

#endif // UTIL_SIMPLE_HASH_TABLE_H__

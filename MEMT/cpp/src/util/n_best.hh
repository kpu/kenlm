#ifndef UTIL_N_BEST_H__
#define UTIL_N_BEST_H__

#include "Share/Debug.hh"

#include<boost/scoped_array.hpp>
#include<boost/iterator/filter_iterator.hpp>
#include<boost/iterator/indirect_iterator.hpp>
#include<boost/optional/optional.hpp>
#include<boost/pending/relaxed_heap.hpp>
#include<boost/ptr_container/indirect_fun.hpp>
#include<boost/unordered_set.hpp>
#include<boost/utility.hpp>
#include<boost/utility/in_place_factory.hpp>

#include<set>
#include<vector>

#include<assert.h>

/* A note on threadsafety: these act like STL classes in that they can be used
 * from different threads, but not simultaneously.  This note applies to const
 * methods as well.  
 */

namespace nbest {
namespace detail {

// Mapping from Value* to [0,max_size] required by boost::relaxed_heap.
template <class Value> struct SubtractPropertyMap : public boost::put_get_helper<size_t, SubtractPropertyMap<Value> > {
	public:
		explicit SubtractPropertyMap(const Value *subtract) : subtract_(subtract) {}
		typedef Value *key_type;
		typedef size_t value_type;
		typedef size_t reference;
		typedef boost::readable_property_map_tag category;
		inline value_type operator[](const key_type &hyp) const { return hyp - subtract_; }
	private:
		const Value *subtract_;
};

// Predicate for skipping available_ using a filter iterator.
template <class Value> class IsNotAvailable {
	public:
		IsNotAvailable(const Value &available) : available_(available) {}

		bool operator()(const Value &value) const {
			return &value != &available_;
		}

	private:
		const Value &available_;
};

// Owns actual values in beam.  Value* pointers thrown around point into here.
template <class ValueT> class ArrayStorage : boost::noncopyable {
	public:
		typedef ValueT Value;

		ArrayStorage() {}

		void Reset(size_t max_size) {
			assert(max_size);
			if (max_size_ != max_size) {
				owner_reset();
				// One more so there's always available.
				owner_.reset(new Value[max_size_ + 1]);
			}
			size_ = 0;
			available_ = owner_.get();
		}

		// Manipulation
		Value &Available() {
			return *available_;
		}
		void UseAvailable() {
			assert(size_ < max_size_);
			++size_;
			++available_;
		}
		void ReplaceAvailable(Value *with) {
			assert(size_ == max_size_);
			available_ = with;
		}

		// Reading: STL-like function names.
		typedef boost::filter_iterator<IsNotAvailable<Value>, Value*> iterator;
		iterator begin() const {
			return iterator(IsNotAvailable<Value>(*available_), base_, base_ + size_ + 1);
		}
		iterator end() const {
			return iterator(IsNotAvailable<Value>(*available_), base_ + size_ + 1, base_ + size_ + 1);
		}
		size_t max_size() const {
			return max_size_;
		}
		size_t size() const {
			return size_;
		}

		// Mapping Value* to offsets in owner_.
		typedef detail::SubtractPropertyMap<Value> PropertyMap;
		PropertyMap GetPropertyMap() const {
			return PropertyMap(owner_.get());
		}

	private:
		size_t max_size_, size_;

		// Owns all hypotheses in the beam.
		// This is not std::vector because Value need not be copyable.
		// This is not boost::array because Reset might change its size.
		// This is not boost::ptr_vector because One::heap_ requires a convenient map into consecutive integers.
		boost::scoped_array<Value> owner_;

		Value *available_;
};

template <class ValueT, class DropperT, class DupeT, class MergeT> class OpenNBest : boost::noncopyable {
	public:
		typedef ArrayStorage<ValueT> Storage;
		typedef ValueT Value;
		typedef DropperT Dropper;
		typedef typename Dropper::Less Less;
		typedef typename DupeT Dupe;
		typedef typename MergeT Merge;

		OpenNBest() {}

		void Reset(const Storage &storage, const Less &less_func, const Merge &merge_func) {
			storage = &storage;
			less_ = boost::in_place(less_func);
			dropper_ = boost::null_t();
			merge_ = boost::in_place(merge_func);
		}

		template <class Score> bool MayMakeIt(const Score &score) const {
			return (storage_->size() < storage_->max_size()) || ((*less_)(heap_->top(), score));
		}

		void InsertAvailable() {
			Value *dupe = dupe_.FindOrInsert(available_);
			if (dupe) {
				if (storage_->size() == storage_->max_size()) {
					typename Dropper::Update update(dropper_, dupe);
					update.Commit(merge_(*dupe, storage_->Available()));
				} else {
					merge_(*dupe, storage_->Available());
				}
			} else {
				if (storage_->size() == storage_->max_size()) {
					dropper_->Insert(storage_->Available());
					Value *removing = dropper_->PopBottom();
					dupe_->Remove(*removing);
					storage_->ReplaceAvailable(removing);
				} else {
					storage_->UseAvailable();
					if (storage_->size() == storage_->max_size()) BuildDropper();
				}
			}
		}

		const Dropper &ConstDropper() const {
			if (!dropper_) {
				BuildDropper();
			}
			return *dropper_;
		}

		Dropper &MutableDropper() {
			if (!dropper_) {
				BuildDropper();
			}
			return *dropper_;
		}

	private:
		void BuildDropper() const {
			dropper_ == boost::inplace(*storage_, less_);
			for (Storage::iterator i = storage_->begin(); i != storage_->end(); ++i) {
				dropper_.insert(*i);
			}
		}

		Storage *storage_;

		boost::optional<Less> less_;

		mutable boost::optional<Dropper> dropper_;

		Dupe dupe_;

		boost::optional<MergeT> merge_;
};

template <class ValueT, class DropperT, class DupeT, class MergeT> class BaseNBest : boost::noncopyable {
	public:
		typedef ArrayStorage<ValueT> Storage;
		typedef ValueT Value;
		typedef DropperT Dropper;
		typedef DupeT Dupe;
		typedef MergeT Merge;
		typedef typename Dropper::Less Less;

		void Reset(size_t max_size, const Less &less_func = Less(), const Dupe &dupe = Dupe(), const Merge &merge = Merge()) {
			storage_.Reset(max_size);
			open_.Reset(storage_, less_func);
			DEBUG_ONLY(destructive_called_ = false);
		}

		Value &Available() { return storage_.Available(); }
		template<class Score> bool MayMakeIt(const Score &score) const {
			DEBUG_ONLY_ASSERT(destructive_called_ == false);
			return open_.MayMakeIt(score);
		}
		void InsertAvailable() {
			DEBUG_ONLY_ASSERT(destructive_called_ == false);
			open_.InsertAvailable();
		}

		size_t max_size() const { return storage_.max_size(); }
		size_t size() const { return storage_.size(); }
		bool empty() const { return size() != 0; }

		typedef Storage::iterator unordered_iterator;
		unordered_iterator unordered_begin() const {
			return storage_.begin();
		}
		unordered_iterator unordered_end() const {
			return storage_.end();
		}

		/* Dump values to a vector sorted in increasing order.  
		 * This operation is destructive.  Reset must be called before the following functions:
		 *   MayMakeIt
		 *   InsertAvailable
		 *   DestructiveIncreasingVector (a second time)
		 *
		 * If compiled without NDEBUG, we check for this.
		 */
		void DestructiveIncreasingVector(std::vector<Value*> &out) {
			DEBUG_ONLY_ASSERT(destructive_called_ == false);
			out.resize(size());
			Dropper &dropper = open_.MutableDropper();
			for (std::vector<Value*>::const_iterator i = out.begin(); i != out.end(); ++i) {
				*i = dropper.PopBottom();
			}
			DEBUG_ONLY(destructive_called_ = true);
		}

	protected:
		BaseNBest() {}

		Storage storage_;
		typedef OpenNBest<ValueT> Open;
		Open open_;

		DEBUG_ONLY(bool destructive_called_);
};

} // namespace detail

/* Useful template arguments for NBest.  It's slightly annoying that these
 * require the repetition of ValueT, but the alternative was passing ValueT
 * as a template argument to every dupe detector and every merger.  In many
 * cases, users write custom classes where ValueT is known, so they need not
 * be templates, and having the ValueT argument would be annoying.  For
 * example, OneBestMerge needs LessT, but other mergers might not, so having
 * NBest copy the template argumet would be annoying.  Finally, having ValueT
 * allows e.g. HashDupe to set defaults for HashT and EqualsT.  
 */

// Used by Droppers.
template <class DropperT> class UpdateBase {
	public:
		typedef DropperT Dropper;
		typedef typename Dropper::Value Value;
		
		~UpdateBase() {
			DEBUG_ONLY_ASSERT(committed_ == true);
		}
		
	protected:
		UpdateBase(Dropper &dropper, Value *value) 
			: dropper_(dropper),
			value_(value),
			DEBUG_ONLY(committed_(false)) {}

		void Commit() {
			DEBUG_ONLY(committed_ = true);
		}

		Dropper &dropper_;
		Value *const value_;

	private:
		DEBUG_ONLY(bool committed_);
};

/* Droppers: These decide what hypothesis to drop from the beam
 * ValueT is a hypothesis type
 * LessT is a less than operator on ValueT.
 * These are one-time use i.e. they have no Reset functionality.
 *
 * Procedure to change the ordering (accoring to LessT) of an entry:
 * 1. Construct Update(*this, value to update)
 * 2. Modify *value as needed.
 * 3. Call Commit indicating if the score possibly changed.
 * Changes which do not impact LessT may be done without notification.
 */
template <class ValueT, class LessT> class HeapDropper : boost::noncopyable {
	private:
		typedef HeapDropper<ValueT, LessT> Self;
	public:
		typedef ValueT Value;
		typedef LessT Less;

		HeapDropper(const detail::ArrayStorage<ValueT> &storage, const Less &less_func)
			// There will be at most max_size() + 1 values in the heap with property map [0, max_size()]
			: heap_(storage.max_size() + 1, PtrLess(less), storage.GetPropertyMap()) {}

		void Insert(Value *value) {
			heap_->push(value);
		}

		Value *PopBottom() {
			Value *ret = heap_->top();
			heap_->pop();
			return ret;
		}
		// Still not threadsafe because top is lazy.
		const Value &PeekBottom() const {
			return *heap_->top();
		}

		class Update : public UpdateBase<Self> {
			private:
				typedef UpdateBase<Self> P;
				
			public:
				Update(Dropper &dropper, Value *value) : P(dropper, value) {}
	
				void Commit(bool score_changed) {
					if (score_changed) P::dropper_.CommitUpdate(P::value_);
					P::Commit();
				}		
		};

	private:
		friend class Update;
		void CommitUpdate(Value *value) {
			heap_->update(value);
		}

		typedef boost::indirect_fun<Less> PtrLess;
		typedef typename detail::ArrayStorageStorage<ValueT>::PropertyMap PropertyMap;
		// Unlike most C++ heap implementations, relaxed_heap's top() is the lowest value.
		typedef boost::relaxed_heap<Value*, PtrLess, PropertyMap> Heap;

		Heap heap_;
};

/* While HeapDropper is generally faster, this allows non-destructive sorted
 * element iteration.  This is useful when insertions into the beam depend on
 * previous insertions.  Otherwise you should probably use HeapDropper and hold
 * onto the destructive sort output.
 */
template <class ValueT, class LessT> class SetDropper : boost::noncopyable {
	private:
		typedef SetDropper<ValueT, LessT> Self;
	public:
		typedef ValueT Value;
		typedef LessT Less;

		SetDropper(const detail::ArrayStorage<ValueT> &storage, const Less &less_func)
			: set_(less_func) {}

		void Insert(Value *value) { set_->insert(value); }

		Value *PopBottom() {
			Value *ret = *set_.begin();
			set_.erase(set_.begin());
			return ret;
		}
		const Value &PeekBottom() const { return **set_.begin(); }

		typedef typename Set::const_iterator const_iterator;
		const_iterator begin() const { return set_.begin(); }
		const_iterator end() const { return set_.end(); }
		typedef typename Set::const_reverse_iterator const_reverse_iterator;
		const_reverse_iterator rbegin() const {	return set_.rbegin(); }
		const_reverse_iterator rend() const { return set_.rend(); }

		class Update : public UpdateBase<Self> {
			private:
				typedef UpdateBase<Self> P;
				
			public:
				Update(Dropper &dropper, Value *value)
					: P(dropper, value), hint_(dropper.PrepareUpdate()) {}
	
				void Commit(bool score_changed) {
					P::dropper_.CommitUpdate(hint_, P::value_);
					P::Commit();
				}
				
			private:
				Set::iterator hint_;
		};

	private:
		friend class Update;
		Set::iterator PrepareUpdate() {
			// TODO: make updates more efficient by using a vector to map from PropertyMap indices to set iterators.
			// Could also keep the iterator as generic data in the dupe detector, but the previous solution is cleaner and faster.
			Set::iterator i(set_.find(value));
			set_.erase(i++);
			return i;
		}
		void CommitUpdate(const Set::iterator &hint, Value *value) {
			set_.insert(hint, value);
		}

		typedef boost::indirect_fun<Less> PtrLess;
		typedef std::set<Value*, PtrLess> Set;
		Set set_;
};

// No dupe detection.
template <class ValueT> class NullDupe {
	public:
		typedef ValueT Value;

		NullDupe() {}

		void Reset() const {}

		// If a dupe of value is found, return it.  Otherwise insert value and return NULL.
		Value *FindOrInsert(Value &value) const {
			return NULL;
		}

		void Remove(Value &value) const {}
};

// Hash table based duplicate detection.
template <class ValueT, class HashT = boost::hash<ValueT>, class EqualsT = std::equals_to<ValueT> > class HashDupe {
	public:
		typedef ValueT Value;
		typedef HashT Hash;
		typedef EqualsT Equals;

		HashDupe(size_t buckets, const Hash &hash_fun, const Equals &equals_fun)
			: dupe_(buckets, hash_fun, equals_fun) {}

		Value *FindOrInsert(Value &value) const {
			std::pair<bool, Dupe::iterator> ret(dupe_.insert(value));
			if (ret.first) return NULL;
			return *ret.second;
		}

		void Remove(Value &value) {
			dupe_.erase(&value);
		}

	private:
		typedef boost::indirect_fun<Hash> PtrHash;
		typedef boost::indirect_fun<Equals> PtrEquals;
		typedef boost::unordered_set<Value*, PtrHash, PtrEquals> Dupe;
		Dupe dupe_;
};

// Merge duplicates by picking the one-best hypothesis.  
template <class ValueT, class LessT> class OneBestMerge : public std::binary_function<ValueT &, const ValueT &, bool> {
	public:
		typedef ValueT Value;
		typedef LessT Less;

	        OneBestMerge() : less_() {}

        	bool operator()(Value &to, const Value &with) const {
	                if (less_(to, with)) {
	                        to = with;
	                        return true;
	                }
	                return false;
	        }

        private:
                const Less less_;
};

// The public nbest list.
template <class ValueT, class DropperT = HeapDropper<ValueT, std::less<ValueT> >, class DupeT = NullDupe<ValueT>, class MergeT = OneBestMerge<ValueT, typename DropperT::Less> > class NBest
		: public detail::BaseNBest<ValueT, DropperT, DupeT, MergeT>,
		  private boost::noncopyable {
	public:
		NBest() {}
};

/*template <class ValueT, class LessT = std::less<ValueT>, class DupeT = NullDupe, class MergeT = OneBestMerge> class SetNBest
		: public detail::BaseNBest<ValueT, detail::SetDropper<ValueT, LessT>, DupeT::In<ValueT>::T, MergeT::In<ValueT, LessT>::T>,
		  private boost::noncopyable {
	private:
		typedef detail::BaseNBest<ValueT, detail::SetDropper<ValueT, LessT>, DupeT::In<ValueT>::T, MergeT::In<ValueT, LessT>::T> P;

	public:
		SetNBest() {}

		// DestructiveIncreasingVector also ruins these calls.
		typedef boost::indirect_iterator<typename P::Dropper::const_iterator> ordered_iterator;
		ordered_iterator ordered_begin() const {
			DEBUG_ONLY_ASSERT(P::destructive_called_ == false);
			return ordered_iterator(GetDropper().begin());
		}
		ordered_iterator ordered_end() const {
			DEBUG_ONLY_ASSERT(P::destructive_called_ == false);
			return ordered_iterator(GetDropper().end());
		}

		typedef boost::indirect_iterator<typename P::Dropper::const_reverse_iterator> ordered_reverse_iterator;
		ordered_reverse_iterator ordered_rbegin() const {
			DEBUG_ONLY_ASSERT(P::destructive_called_ == false);
			return ordered_reverse_iterator(GetDropper().rbegin());
		}
		ordered_reverse_iterator ordered_rend() const {
			DEBUG_ONLY_ASSERT(P::destructive_called_ == false);
			return ordered_reverse_iterator(GetDropper().rend());
		}

	private:
		const detail::SetDropper<ValueT, LessT> &GetDropper() const {
			return P::open_::ConstDropper();
		}
};*/

} // namespace nbest

#endif // UTIL_N_BEST_H__

#ifndef UTIL_N_BEST_H__
#define UTIL_N_BEST_H__

#include "Share/Debug.hh"

#include<boost/functional/hash.hpp>
#include<boost/iterator/filter_iterator.hpp>
#include<boost/iterator/indirect_iterator.hpp>
#include<boost/optional/optional.hpp>
#include<boost/pending/relaxed_heap.hpp>
#include<boost/ptr_container/indirect_fun.hpp>
#include<boost/scoped_array.hpp>
#include<boost/unordered/unordered_set.hpp>
#include<boost/utility.hpp>
#include<boost/utility/in_place_factory.hpp>

#include<set>
#include<vector>

#include<assert.h>

/* A note on threadsafety: these act like STL classes in that they can be used
 * from different threads, but not simultaneously.  This note applies to const
 * methods as well.  
 * Scroll down to the bottom for the public classes.
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
		IsNotAvailable(const Value &available) : available_(&available) {}

		bool operator()(const Value &value) const {
			return &value != available_;
		}

	private:
		const Value *available_;
};

// Owns actual values in beam.  Value* pointers thrown around point into here.
template <class ValueT> class ArrayStorage : boost::noncopyable {
	public:
		typedef ValueT Value;

		ArrayStorage() {}

		void Reset(size_t max_size) {
			assert(max_size);
			if (max_size_ != max_size) {
				max_size_ = max_size;
				owner_.reset();
				// One more so there's always available.
				owner_.reset(new Value[max_size + 1]);
			}
			size_ = 0;
			available_ = owner_.get();
		}

		// Manipulation
		Value &Available() {
			return *available_;
		}
		const Value &ConstAvailable() const {
			return *available_;
		}
		void UseAvailable() {
			assert(size_ < max_size_);
			++size_;
			++available_;
		}
		void ReplaceAvailable(Value &with) {
			assert(size_ == max_size_);
			available_ = &with;
		}

		// Reading: STL-like function names.
		typedef boost::filter_iterator<IsNotAvailable<Value>, Value*> iterator;
		iterator begin() const {
			return iterator(IsNotAvailable<Value>(*available_), owner_.get(), owner_.get() + size_ + 1);
		}
		iterator end() const {
			return iterator(IsNotAvailable<Value>(*available_), owner_.get() + size_ + 1, owner_.get() + size_ + 1);
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

// Used by Droppers.
template <class DropperT> class UpdateBase : boost::noncopyable {
	public:
		typedef DropperT Dropper;
		typedef typename Dropper::Value Value;
		
		~UpdateBase() {
			DEBUG_ONLY_ASSERT(committed_ == true);
		}
		
	protected:
		UpdateBase(Dropper &dropper, Value &value) 
				: dropper_(dropper),
				  value_(value) {
			DEBUG_ONLY(committed_ = false);
		}

		void Commit() {
			DEBUG_ONLY(committed_ = true);
		}

		Dropper &dropper_;
		Value &value_;

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
 *
 * TODO: replace this with a min-max heap from
 *   Atkinson et al, Min-max heaps and generalized priority queues, 1986
 */
template <class ValueT, class LessT> class HeapDropper : boost::noncopyable {
	private:
		typedef HeapDropper<ValueT, LessT> Self;
	public:
		typedef ValueT Value;
		typedef LessT Less;

		HeapDropper(const detail::ArrayStorage<ValueT> &storage, const Less &less_func)
			// There will be at most max_size() + 1 values in the heap with property map [0, max_size()]
			: heap_(storage.max_size() + 1, PtrLess(less_func), storage.GetPropertyMap()) {}

		void Insert(Value &value) {
			heap_.push(&value);
		}

		Value &PopBottom() {
			Value *ret = heap_.top();
			heap_.pop();
			return *ret;
		}
		// Still not reentrant.
		const Value &PeekBottom() const {
			return *heap_.top();
		}

		class Update : public UpdateBase<Self> {
			private:
				typedef UpdateBase<Self> P;
				
			public:
				Update(Self &dropper, Value &value) : P(dropper, value) {
					dropper.PrepareUpdate(value);
				}
	
				void Commit(bool score_changed) {
					P::dropper_.CommitUpdate(P::value_);
					P::Commit();
				}		
		};

	private:
		friend class Update;
		void PrepareUpdate(Value &value) {
			// Ugh relaxed_heap::update only allows decreasing score, but updates usually increase.
			heap_.remove(&value);
		}
		void CommitUpdate(Value &value) {
			heap_.push(&value);
		}

		typedef boost::indirect_fun<Less> PtrLess;
		typedef typename detail::ArrayStorage<ValueT>::PropertyMap PropertyMap;

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
	private:
		class FullLess : public std::binary_function<const Value *, const Value *, bool> {
			public:
				FullLess(const Less &less_func) : less_(less_func) {}

				bool operator()(const Value *left, const Value *right) const {
					if (less_(*left, *right)) return true;
					if (less_(*right, *left)) return false;
					return left < right;
				}

			private:
				const Less less_;
		};

		typedef std::set<Value*, FullLess> Set;

	public:

		SetDropper(const detail::ArrayStorage<ValueT> &storage, const Less &less_func)
			: set_(FullLess(less_func)) {}

		void Insert(Value &value) { set_.insert(&value); }

		Value &PopBottom() {
			Value &ret = **set_.begin();
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
				Update(Self &dropper, Value &value)
					: P(dropper, value), hint_(dropper.PrepareUpdate(value)) {}
	
				void Commit(bool score_changed) {
					P::dropper_.CommitUpdate(hint_, P::value_);
					P::Commit();
				}
				
			private:
				typename Set::iterator hint_;
		};

	private:
		friend class Update;

		typedef typename Set::iterator iterator;

		iterator PrepareUpdate(Value &value) {
			// TODO: make updates more efficient by using a vector to map from PropertyMap indices to set iterators.
			// Could also keep the iterator as generic data in the dupe detector, but the previous solution is cleaner and faster.
			iterator i(set_.find(&value));
			assert(i != set_.end());
			set_.erase(i++);
			return i;
		}
		void CommitUpdate(const typename Set::iterator &hint, Value &value) {
			set_.insert(hint, &value);
		}

		Set set_;
};

template <class ValueT, class DropperT, class DupeT, class MergeT> class OpenNBest : boost::noncopyable {
	public:
		typedef ArrayStorage<ValueT> Storage;
		typedef ValueT Value;
		typedef DropperT Dropper;
		typedef typename Dropper::Less Less;
		typedef DupeT Dupe;
		typedef MergeT Merge;

		OpenNBest() {}

		void Reset(
				Storage &storage,
				const Less &less_func,
				const Dupe &dupe_func,
				const Merge &merge_func) {
			storage_ = &storage;
			less_ = boost::in_place(less_func);
			dropper_ = boost::none_t();
			dupe_func.Construct(dupe_, storage.max_size());
			merge_ = boost::in_place(merge_func);
		}

		template <class Score> bool MayMakeIt(const Score &score) const {
			return (storage_->size() < storage_->max_size()) || ((*less_)(dropper_->PeekBottom(), score));
		}

		void InsertAvailable() {
			Value *dupe = dupe_->FindOrInsert(storage_->Available());
			if (dupe) {
				if (storage_->size() == storage_->max_size()) {
					typename Dropper::Update update(*dropper_, *dupe);
					update.Commit((*merge_)(*dupe, storage_->ConstAvailable()));
				} else {
					(*merge_)(*dupe, storage_->ConstAvailable());
				}
			} else {
				if (storage_->size() == storage_->max_size()) {
					dropper_->Insert(storage_->Available());
					Value &removing = dropper_->PopBottom();
					dupe_->Remove(removing);
					storage_->ReplaceAvailable(removing);
				} else {
					storage_->UseAvailable();
					// The Dropper is built when the beam is filled, not when it needs to evict.  Sue me.  
					if (storage_->size() == storage_->max_size()) BuildDropper();
				}
			}
		}

		const Dropper &ConstDropper() const {
			BuildDropper();
			return *dropper_;
		}

		Dropper &MutableDropper() {
			BuildDropper();
			return *dropper_;
		}

	private:
		void BuildDropper() const {
			if (dropper_) return;
			dropper_ = boost::in_place(*storage_, *less_);
			Dropper &dropper = *dropper_;
			for (typename Storage::iterator i = storage_->begin(); i != storage_->end(); ++i) {
				dropper.Insert(*i);
			}
		}

		Storage *storage_;

		boost::optional<Less> less_;

		mutable boost::optional<Dropper> dropper_;

		boost::optional<typename Dupe::Impl> dupe_;

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

		void Reset(size_t max_size, const Less &less_func = Less(), const Dupe &dupe_func = Dupe(), const Merge &merge_func = Merge()) {
			storage_.Reset(max_size);
			open_.Reset(storage_, less_func, dupe_func, merge_func);
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
		bool empty() const { return size() == 0; }

		typedef typename Storage::iterator unordered_iterator;
		unordered_iterator unordered_begin() const {
			return storage_.begin();
		}
		unordered_iterator unordered_end() const {
			return storage_.end();
		}

	protected:
		BaseNBest() {}

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
			for (typename std::vector<Value*>::iterator i = out.begin(); i != out.end(); ++i) {
				*i = &dropper.PopBottom();
			}
			DEBUG_ONLY(destructive_called_ = true);
		}

		Storage storage_;
		typedef OpenNBest<ValueT, DropperT, DupeT, MergeT> Open;
		Open open_;

		DEBUG_ONLY(bool destructive_called_);
};

// No dupe detection.
template <class ValueT> class NullDupeImpl : boost::noncopyable {
	public:
		typedef ValueT Value;

		NullDupeImpl() {}

		// If a dupe of value is found, return it.  Otherwise insert value and return NULL.
		Value *FindOrInsert(Value &value) const {
			return NULL;
		}

		void Remove(Value &value) const {}
};

// Hash table based duplicate detection.
template <class ValueT, class HashT, class EqualsT> class HashDupeImpl : boost::noncopyable {
	public:
		typedef ValueT Value;
		typedef HashT Hash;
		typedef EqualsT Equals;

		HashDupeImpl(size_t max_size, const Hash &hash_fun, const Equals &equals_fun)
			: dupe_(max_size, hash_fun, equals_fun) {}

		Value *FindOrInsert(Value &value) {
			std::pair<typename Dupe::const_iterator, bool> ret(dupe_.insert(&value));
			if (ret.second) return NULL;
			return *ret.first;
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

// No dupe detection.
template <class ValueT> class NullDupe {
	public:
		typedef ValueT Value;
		typedef detail::NullDupeImpl<Value> Impl;

		NullDupe() {}

		void Construct(boost::optional<Impl> &out, size_t max_size) const {
			out = boost::in_place();
		}
};

// Hash table based duplicate detection.
template <class ValueT, class HashT = boost::hash<ValueT>, class EqualsT = std::equal_to<ValueT> > class HashDupe {
	public:
		typedef ValueT Value;
		typedef HashT Hash;
		typedef EqualsT Equals;
		typedef detail::HashDupeImpl<Value, Hash, Equals> Impl;

		HashDupe(const Hash &hash_func = Hash(), const Equals &equals_func = Equals())
			: hash_(hash_func), equals_(equals_func) {}

		void Construct(boost::optional<Impl> &out, size_t max_size) const {
			out = boost::in_place(max_size, hash_, equals_);
		}

	private:
		const Hash hash_;
		const Equals equals_;
};


// Merge duplicates by picking the one-best hypothesis.  
template <class ValueT, class LessT> class OneBestMerge : public std::binary_function<ValueT &, const ValueT &, bool> {
	public:
		typedef ValueT Value;
		typedef LessT Less;

	        OneBestMerge(const Less &less = Less()) : less_(less) {}

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

template <class ValueT> class NullMerge : public std::binary_function<ValueT &, const ValueT &, bool> {
	public:
		typedef ValueT Value;

		NullMerge() {}

		bool operator()(Value &to, const Value &with) const {
			return false;
		}
};

/* The public NBest classes.
 * ValueT is the value to store.
 * LessT compares scores for purposes of dropping values and ordered iteration.
 * DupeT is the duplicate detection policy, usually NullDupe<ValueT> or
 *   HashDupe<ValueT, HashT, EqualsT>.  Note that HashT and EqualsT are for
 *   duplicate detection only.  In particular, hypotheses equal accoring to
 *   EqualsT are duplicates but may score differently accoring to LessT.
 *   If you set DupeT, you want to set MergeT as well.
 * MergeT is a binary functor that describes how to merge duplicate hypotheses
 *   found by DupeT.  The right argument should be merged into the left
 *   argument, returning true if the score used by LessT may have changed.
 *   See OneBestMerge for an example.  
 *   Currently MergeT must have the correct API but will not be called if DupeT
 *   is NullDupe<ValueT>.  In that case you may as well use the default DupeT
 *   and MergeT.   
 *
 * Setup
 * Default constructor.
 * Reset(size_t, LessT, DupeT, MergeT) to set the max size and construct the
 *   visitors.
 *
 * Insertion
 * Available() to get a reference to a hypothesis in contructed but otherwise
 *   unspecified state.
 * MayMakeIt(anything that LessT accepts as a second argument).  If it returns
 *   false then the score is too low to make it.  This call is optional.
 * InsertAvailable() inserts Available()
 *
 * Sizes
 * max_size(), size(), and empty()
 *
 * Reading
 * Unordered using unordered_iterator returned by unordered_begin and
 *   unordered_end.  
 * Ordered depends on NBest or SortedNBest, as documented below.
 */
template <class ValueT, class LessT = std::less<ValueT>, class DupeT = NullDupe<ValueT>, class MergeT = NullMerge<ValueT> > class NBest
		: public detail::BaseNBest<ValueT, detail::HeapDropper<ValueT, LessT>, DupeT, MergeT> {
	private:
		typedef detail::BaseNBest<ValueT, detail::HeapDropper<ValueT, LessT>, DupeT, MergeT> P;
		typedef std::vector<ValueT*> Ordered;

	public:
		NBest() {}

		void Reset(
				size_t max_size,
				const typename P::Less &less_func = typename P::Less(),
				const typename P::Dupe &dupe_func = typename P::Dupe(),
				const typename P::Merge &merge_func = typename P::Merge()) {
			P::Reset(max_size, less_func, dupe_func, merge_func);
			ordered_ = boost::none_t();
		}

		// This is destructive: Reset must be called before MayMakeIt or InsertAvailable.
		void destructive_ordered_make() {
			if (ordered_) return;
			ordered_ = boost::in_place();
			P::DestructiveIncreasingVector(*ordered_);
		}
		// Iterator interface requires destructive_ordered_make was called since Reset.
		typedef boost::indirect_iterator<typename Ordered::const_iterator> increasing_iterator;
		increasing_iterator destructive_increasing_begin() const {
			return increasing_iterator(ordered_->begin());
		}
		increasing_iterator destructive_increasing_end() const {
			return increasing_iterator(ordered_->end());
		}
		// Iterator interface requires destructive_ordered_make was called since Reset.
		typedef boost::indirect_iterator<typename Ordered::const_reverse_iterator> decreasing_iterator;
		decreasing_iterator destructive_decreasing_begin() const {
			return decreasing_iterator(ordered_->rbegin());
		}
		decreasing_iterator destructive_decreasing_end() const {
			return decreasing_iterator(ordered_->rend());
		}

	private:
		boost::optional<Ordered> ordered_;
};

/* Unlike NBest, SortedNBest is always sorted and provides non-destructive ordered iteration */
template <class ValueT, class LessT = std::less<ValueT>, class DupeT = NullDupe<ValueT>, class MergeT = OneBestMerge<ValueT, LessT> > class SortedNBest
		: public detail::BaseNBest<ValueT, detail::SetDropper<ValueT, LessT>, DupeT, MergeT> {
	private:
		typedef detail::BaseNBest<ValueT, detail::SetDropper<ValueT, LessT>, DupeT, MergeT> P;

	public:
		SortedNBest() {}

		typedef boost::indirect_iterator<typename P::Dropper::const_iterator> increasing_iterator;
		increasing_iterator increasing_begin() const {
			DEBUG_ONLY_ASSERT(P::destructive_called_ == false);
			return increasing_iterator(GetDropper().begin());
		}
		increasing_iterator increasing_end() const {
			DEBUG_ONLY_ASSERT(P::destructive_called_ == false);
			return increasing_iterator(GetDropper().end());
		}
		typedef boost::indirect_iterator<typename P::Dropper::const_reverse_iterator> decreasing_iterator;
		decreasing_iterator decreasing_begin() const {
			DEBUG_ONLY_ASSERT(P::destructive_called_ == false);
			return decreasing_iterator(GetDropper().rbegin());
		}
		decreasing_iterator decreasing_end() const {
			DEBUG_ONLY_ASSERT(P::destructive_called_ == false);
			return decreasing_iterator(GetDropper().rend());
		}

	private:
		const detail::SetDropper<ValueT, LessT> &GetDropper() const {
			return P::open_::ConstDropper();
		}
};

} // namespace nbest

#endif // UTIL_N_BEST_H__

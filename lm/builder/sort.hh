#ifndef LM_BUILDER_SORT_H
#define LM_BUILDER_SORT_H

#include "lm/builder/ngram_stream.hh"
#include "lm/builder/ngram.hh"
#include "lm/word_index.hh"
#include "util/stream/sort.hh"

#include "util/stream/timer.hh"

#include <functional>
#include <string>

namespace lm {
namespace builder {

/**
 * Abstract parent class for defining custom n-gram comparators.
 */
template <class Child> class Comparator : public std::binary_function<const void *, const void *, bool> {
  public:
  
    /**
     * Constructs a comparator capable of comparing two n-grams.
     *
     * @param order Number of words in each n-gram
     */
    explicit Comparator(std::size_t order) : order_(order) {}

    /**
     * Applies the comparator using the Compare method that must be defined in any class that inherits from this class.
     *
     * @param lhs A pointer to the n-gram on the left-hand side of the comparison
     * @param rhs A pointer to the n-gram on the right-hand side of the comparison
     *
     * @see ContextOrder::Compare
     * @see PrefixOrder::Compare
     * @see SuffixOrder::Compare
     */
    inline bool operator()(const void *lhs, const void *rhs) const {
      return static_cast<const Child*>(this)->Compare(static_cast<const WordIndex*>(lhs), static_cast<const WordIndex*>(rhs));
    }

    /** Gets the n-gram order defined for this comparator. */
    std::size_t Order() const { return order_; }

  protected:
    std::size_t order_;
};

/**
 * N-gram comparator that compares n-grams according to their reverse (suffix) order.
 *
 * This comparator compares n-grams lexicographically, one word at a time, 
 * beginning with the last word of each n-gram and ending with the first word of each n-gram. 
 *
 * Some examples of n-gram comparisons as defined by this comparator:
 * - a b c == a b c
 * - a b c < a b d
 * - a b c > a d b
 * - a b c > a b b
 * - a b c > x a c
 * - a b c < x y z
 */
class SuffixOrder : public Comparator<SuffixOrder> {
  public:
  
    /** 
     * Constructs a comparator capable of comparing two n-grams.
     *
     * @param order Number of words in each n-gram
     */
    explicit SuffixOrder(std::size_t order) : Comparator<SuffixOrder>(order) {}

    /**
     * Compares two n-grams lexicographically, one word at a time, 
     * beginning with the last word of each n-gram and ending with the first word of each n-gram.
     *
     * @param lhs A pointer to the n-gram on the left-hand side of the comparison
     * @param rhs A pointer to the n-gram on the right-hand side of the comparison
     */
    inline bool Compare(const WordIndex *lhs, const WordIndex *rhs) const {
      for (std::size_t i = order_ - 1; i != 0; --i) {
        if (lhs[i] != rhs[i])
          return lhs[i] < rhs[i];
      }
      return lhs[0] < rhs[0];
    }

    static const unsigned kMatchOffset = 1;
};

  
/**
  * N-gram comparator that compares n-grams according to the reverse (suffix) order of the n-gram context.
  *
  * This comparator compares n-grams lexicographically, one word at a time, 
  * beginning with the penultimate word of each n-gram and ending with the first word of each n-gram;
  * finally, this comparator compares the last word of each n-gram.
  *
  * Some examples of n-gram comparisons as defined by this comparator:
  * - a b c == a b c
  * - a b c < a b d
  * - a b c < a d b
  * - a b c > a b b
  * - a b c > x a c
  * - a b c < x y z
  */
class ContextOrder : public Comparator<ContextOrder> {
  public:
  
    /** 
     * Constructs a comparator capable of comparing two n-grams.
     *
     * @param order Number of words in each n-gram
     */
    explicit ContextOrder(std::size_t order) : Comparator<ContextOrder>(order) {}

    /**
     * Compares two n-grams lexicographically, one word at a time, 
     * beginning with the penultimate word of each n-gram and ending with the first word of each n-gram;
     * finally, this comparator compares the last word of each n-gram.
     *
     * @param lhs A pointer to the n-gram on the left-hand side of the comparison
     * @param rhs A pointer to the n-gram on the right-hand side of the comparison
     */
    inline bool Compare(const WordIndex *lhs, const WordIndex *rhs) const {
      for (int i = order_ - 2; i >= 0; --i) {
        if (lhs[i] != rhs[i])
          return lhs[i] < rhs[i];
      }
      return lhs[order_ - 1] < rhs[order_ - 1];
    }
};

/**
 * N-gram comparator that compares n-grams according to their natural (prefix) order.
 *
 * This comparator compares n-grams lexicographically, one word at a time, 
 * beginning with the first word of each n-gram and ending with the last word of each n-gram.
 *
 * Some examples of n-gram comparisons as defined by this comparator:
 * - a b c == a b c
 * - a b c < a b d
 * - a b c < a d b
 * - a b c > a b b
 * - a b c < x a c
 * - a b c < x y z
 */
class PrefixOrder : public Comparator<PrefixOrder> {
  public:
  
    /** 
     * Constructs a comparator capable of comparing two n-grams.
     *
     * @param order Number of words in each n-gram
     */
    explicit PrefixOrder(std::size_t order) : Comparator<PrefixOrder>(order) {}

    /**
     * Compares two n-grams lexicographically, one word at a time, 
     * beginning with the first word of each n-gram and ending with the last word of each n-gram.
     *
     * @param lhs A pointer to the n-gram on the left-hand side of the comparison
     * @param rhs A pointer to the n-gram on the right-hand side of the comparison
     */
    inline bool Compare(const WordIndex *lhs, const WordIndex *rhs) const {
      for (std::size_t i = 0; i < order_; ++i) {
        if (lhs[i] != rhs[i])
          return lhs[i] < rhs[i];
      }
      return false;
    }
    
    static const unsigned kMatchOffset = 0;
};

// Sum counts for the same n-gram.
struct AddCombiner {
  bool operator()(void *first_void, const void *second_void, const SuffixOrder &compare) const {
    NGram first(first_void, compare.Order());
    // There isn't a const version of NGram.  
    NGram second(const_cast<void*>(second_void), compare.Order());
    if (memcmp(first.begin(), second.begin(), sizeof(WordIndex) * compare.Order())) return false;
    first.Count() += second.Count();
    return true;
  }
};

// The combiner is only used on a single chain, so I didn't bother to allow
// that template.
/**
 * Represents an @ref util::FixedArray "array" capable of storing @ref util::stream::Sort "Sort" objects.
 *
 * In the anticipated use case, an instance of this class will maintain one @ref util::stream::Sort "Sort" object
 * for each n-gram order (ranging from 1 up to the maximum n-gram order being processed).
 * Use in this manner would enable the n-grams each n-gram order to be sorted, in parallel.
 *
 * @tparam Compare An @ref Comparator "ngram comparator" to use during sorting.
 */
template <class Compare> class Sorts : public util::FixedArray<util::stream::Sort<Compare> > {
  private:
    typedef util::stream::Sort<Compare> S;
    typedef util::FixedArray<S> P;

  public:
  
    /**
     * Constructs, but does not initialize.
     * 
     * @ref util::FixedArray::Init() "Init" must be called before use.
     *
     * @see util::FixedArray::Init()
     */
    Sorts() {}

    /**
     * Constructs an @ref util::FixedArray "array" capable of storing a fixed number of @ref util::stream::Sort "Sort" objects.
     *
     * @param number The maximum number of @ref util::stream::Sort "sorters" that can be held by this @ref util::FixedArray "array"
     * @see util::FixedArray::FixedArray()
     */
    explicit Sorts(std::size_t number) : util::FixedArray<util::stream::Sort<Compare> >(number) {}

    /** 
     * Constructs a new @ref util::stream::Sort "Sort" object which is stored in this @ref util::FixedArray "array".
     *
     * The new @ref util::stream::Sort "Sort" object is constructed using the provided @ref util::stream::SortConfig "SortConfig" and @ref Comparator "ngram comparator";
     * once constructed, a new worker @ref util::stream::Thread "thread" (owned by the @ref util::stream::Chain "chain") will sort the n-gram data stored
     * in the @ref util::stream::Block "blocks" of the provided @ref util::stream::Chain "chain".
     *
     * @see util::stream::Sort::Sort()
     * @see util::stream::Chain::operator>>()
     */
    void push_back(util::stream::Chain &chain, const util::stream::SortConfig &config, const Compare &compare) {
      new (P::end()) S(chain, config, compare); // use "placement new" syntax to initalize S in an already-allocated memory location
      P::Constructed();
    }
};

} // namespace builder
} // namespace lm

#endif // LM_BUILDER_SORT_H

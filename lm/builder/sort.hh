#ifndef LM_BUILDER_SORT__
#define LM_BUILDER_SORT__

#include "lm/builder/multi_stream.hh"
#include "lm/builder/ngram.hh"
#include "lm/word_index.hh"
#include "util/stream/sort.hh"

#include <functional>

namespace lm {
namespace builder {

template <class Child> class Comparator : public std::binary_function<const void *, const void *, bool> {
  public:
    explicit Comparator(std::size_t order) : order_(order) {}

    inline bool operator()(const void *lhs, const void *rhs) const {
      return static_cast<const Child*>(this)->Compare(static_cast<const WordIndex*>(lhs), static_cast<const WordIndex*>(rhs));
    }

    std::size_t Order() const { return order_; }

  protected:
    std::size_t order_;
};

class SuffixOrder : public Comparator<SuffixOrder> {
  public:
    explicit SuffixOrder(std::size_t order) : Comparator<SuffixOrder>(order) {}

    inline bool Compare(const WordIndex *lhs, const WordIndex *rhs) const {
      for (std::size_t i = order_ - 1; i != 0; --i) {
        if (lhs[i] != rhs[i])
          return lhs[i] < rhs[i];
      }
      return lhs[0] < rhs[0];
    }
};

class ContextOrder : public Comparator<SuffixOrder> {
  public:
    explicit ContextOrder(std::size_t order) : Comparator<SuffixOrder>(order) {}

    inline bool Compare(const WordIndex *lhs, const WordIndex *rhs) const {
      for (int i = order_ - 2; i >= 0; --i) {
        if (lhs[i] != rhs[i])
          return lhs[i] < rhs[i];
      }
      return lhs[order_ - 1] < rhs[order_ - 1];
    }
};

struct AddCombiner {
  bool operator()(void *first_void, void *second_void, const SuffixOrder &compare) {
    NGram first(first_void, compare.Order()), second(second_void, compare.Order());
    if (!memcmp(first.begin(), second.begin(), sizeof(WordIndex) * compare.Order())) return false;
    first.Count() += second.Count();
    return true;
  }
};


// And now how to use these in multiple streams.  
template <class Compare> class Sorts;

// Annoying separate types so operator>> can tell the difference.  
template <class Compare> struct UnsortedRet { Sorts<Compare> *master; };
template <class Compare> struct SortedRet { Sorts<Compare> *master; };

template <class Compare> class Sorts : public FixedArray<util::stream::Sort<Compare> > {
  private:
    typedef FixedArray<util::stream::Sort<Compare> > P;
  public:
    explicit Sorts(const util::stream::SortConfig &config) : config_(config) {}

    void Unsorted(Chains &chains) {
      P::Init(chains.size());
      for (util::stream::Chain *c = chains.begin(); c != chains.end(); ++c) {
        util::stream::SortConfig config(config_);
        config.chain.entry_size = c->EntrySize();
        new (P::end()) util::stream::Sort<Compare>(config, Compare(c - chains.begin() + 1));
        P::Constructed();
        *c >> (P::end() - 1)->Unsorted();
      }
    }

    void Sorted(Chains &chains) {
      for (size_t i = 0; i < chains.size(); ++i) {
        chains[i] >> (*this)[i].Sorted();
      }
    }

    UnsortedRet<Compare> Unsorted() {
      UnsortedRet<Compare> ret;
      ret.master = this;
      return ret;
    }

    SortedRet<Compare> Sorted() {
      SortedRet<Compare> ret;
      ret.master = this;
      return ret;
    }

  private:
    util::stream::SortConfig config_;
};

// Yes it's ugly to be defining class methods here.  But if I defined them as 
// separate functions, they were not working with
// Chains(config) >> sorter.Sorted()
// for whatever silly reason.  
template <class Compare> Chains &Chains::operator>>(const UnsortedRet<Compare> &unsorted) {
  unsorted.master->Unsorted(*this);
  return *this;
}
template <class Compare> Chains &Chains::operator>>(const SortedRet<Compare> &sorted) {
  sorted.master->Sorted(*this);
  return *this;
}

} // namespace builder
} // namespace lm

#endif // LM_BUILDER_SORT__

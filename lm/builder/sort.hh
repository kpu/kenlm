#ifndef LM_BUILDER_SORT__
#define LM_BUILDER_SORT__

#include "lm/builder/multi_stream.hh"
#include "lm/builder/ngram.hh"
#include "lm/word_index.hh"
#include "util/stream/sort.hh"

#include "util/stream/timer.hh"

#include <functional>
#include <string>

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

    static const unsigned kMatchOffset = 1;
};

class ContextOrder : public Comparator<ContextOrder> {
  public:
    explicit ContextOrder(std::size_t order) : Comparator<ContextOrder>(order) {}

    inline bool Compare(const WordIndex *lhs, const WordIndex *rhs) const {
      for (int i = order_ - 2; i >= 0; --i) {
        if (lhs[i] != rhs[i])
          return lhs[i] < rhs[i];
      }
      return lhs[order_ - 1] < rhs[order_ - 1];
    }
};

class PrefixOrder : public Comparator<PrefixOrder> {
  public:
    explicit PrefixOrder(std::size_t order) : Comparator<PrefixOrder>(order) {}

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
  bool operator()(void *first_void, void *second_void, const SuffixOrder &compare) {
    NGram first(first_void, compare.Order()), second(second_void, compare.Order());
    if (memcmp(first.begin(), second.begin(), sizeof(WordIndex) * compare.Order())) return false;
    first.Count() += second.Count();
    return true;
  }
};

// The combiner is only used on a single chain, so I didn't bother to allow
// that template.  
template <class Compare> class Sorts : private FixedArray<util::stream::Sort<Compare> > {
  private:
    typedef util::stream::Sort<Compare> S;
    typedef FixedArray<S> P;

  public:
    Sorts(util::stream::FileBuffer &unigrams, Chains &chains, const util::stream::SortConfig &config)
      : P(chains.size() - 1), unigrams_(unigrams) {
      chains[0] >> unigrams_.Sink();
      for (util::stream::Chain *i = chains.begin() + 1; i != chains.end(); ++i) {
        new (P::end()) S(*i, config, Compare(i - chains.begin() + 1));
        P::Constructed();
      }
    }

    struct TwoReaders {
      TwoReaders(int fd) : ahead(util::DupOrThrow(fd), true), behind(fd, true) {}
      TwoReaders(util::stream::FileBuffer &buffer) 
        : ahead(buffer.Source()), behind(buffer.Source()) {}
      util::stream::PRead ahead;
      util::stream::PRead behind;
    };

    TwoReaders OutputTwice(std::size_t index) {
      if (index == 0) return TwoReaders(unigrams_);
      return TwoReaders((*this)[index - 1].StealCompleted());
    }

    void Output(Chains &chains) {
      assert(chains.size() == 1 + P::size());
      chains[0] >> unigrams_.Source();
      for (size_t i = 1; i < chains.size(); ++i) {
        P::begin()[i - 1].Output(chains[i]);
      }
    }

  private:
    util::stream::FileBuffer &unigrams_;
};

  template <class Compare> void BlockingSort(util::stream::FileBuffer &unigrams, Chains &chains, const util::stream::SortConfig &config, const std::string &timer_name) {
  Sorts<Compare> sorts(unigrams, chains, config);
  {
    UTIL_TIMER("Barrier for %w seconds\n");
    chains.Wait(true);
  }
  {
    UTIL_TIMER(timer_name + ": Non-lazy merge sort took %w seconds\n");
    sorts.Output(chains);
  }
}

} // namespace builder
} // namespace lm

#endif // LM_BUILDER_SORT__

// Convenience to sort multiple streams
#ifndef LM_BUILDER_MULTI_SORT__
#define LM_BUILDER_MULTI_SORT__

#include "lm/builder/multi_stream.hh"
#include "util/stream/sort.hh"

namespace lm { namespace builder {

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

template <class Compare> Chains &operator>>(Chains &chains, UnsortedRet<Compare> unsorted) {
  unsorted.master->Unsorted(chains);
  return chains;
}
template <class Compare> Chains &operator>>(Chains &chains, SortedRet<Compare> sorted) {
  sorted.master->Sorted(chains);
  return chains;
}

}} // namespaces

#endif // LM_BUILDER_MULTI_SORT__

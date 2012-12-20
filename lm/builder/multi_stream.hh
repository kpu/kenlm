#ifndef LM_BUILDER_MULTI_STREAM__
#define LM_BUILDER_MULTI_STREAM__

#include "lm/builder/ngram_stream.hh"
#include "util/scoped.hh"
#include "util/stream/chain.hh"

#include <cstddef>
#include <new>

#include <assert.h>
#include <stdlib.h>

namespace lm { namespace builder {

template <class T> class FixedArray {
  public:
    explicit FixedArray(std::size_t count) {
      Init(count);
    }

    FixedArray() : newed_end_(NULL) {}

    void Init(std::size_t count) {
      assert(!block_.get());
      block_.reset(malloc(sizeof(T) * count));
      if (!block_.get()) throw std::bad_alloc();
      newed_end_ = begin();
    }

    FixedArray(const FixedArray &from) {
      std::size_t size = from.newed_end_ - static_cast<const T*>(from.block_.get());
      Init(size);
      for (std::size_t i = 0; i < size; ++i) {
        new(end()) T(from[i]);
        Constructed();
      }
    }

    ~FixedArray() { clear(); }

    T *begin() { return static_cast<T*>(block_.get()); }
    const T *begin() const { return static_cast<const T*>(block_.get()); }
    // Always call Constructed after successful completion of new.  
    T *end() { return newed_end_; }
    const T *end() const { return newed_end_; }

    T &back() { return *(end() - 1); }
    const T &back() const { return *(end() - 1); }

    std::size_t size() const { return end() - begin(); }
    bool empty() const { return begin() == end(); }

    T &operator[](std::size_t i) { return begin()[i]; }
    const T &operator[](std::size_t i) const { return begin()[i]; }

    template <class C> void push_back(const C &c) {
      new (end()) T(c);
      Constructed();
    }

    void clear() {
      for (T *i = begin(); i != end(); ++i)
        i->~T();
      newed_end_ = begin();
    }

  protected:
    void Constructed() {
      ++newed_end_;
    }

  private:
    util::scoped_malloc block_;

    T *newed_end_;
};

class Chains;

class ChainPositions : public FixedArray<util::stream::ChainPosition> {
  public:
    ChainPositions() {}

    void Init(Chains &chains);

    explicit ChainPositions(Chains &chains) {
      Init(chains);
    }
};

class Chains : public FixedArray<util::stream::Chain> {
  private:
    template <class T, void (T::*ptr)(const ChainPositions &) = &T::Run> struct CheckForRun {
      typedef Chains type;
    };

  public:
    explicit Chains(std::size_t limit) : FixedArray<util::stream::Chain>(limit) {}

    template <class Worker> typename CheckForRun<Worker>::type &operator>>(const Worker &worker) {
      threads_.push_back(new util::stream::Thread(ChainPositions(*this), worker));
      return *this;
    }

    template <class Worker> typename CheckForRun<Worker>::type &operator>>(const boost::reference_wrapper<Worker> &worker) {
      threads_.push_back(new util::stream::Thread(ChainPositions(*this), worker));
      return *this;
    }

    Chains &operator>>(const util::stream::Recycler &recycler) {
      for (util::stream::Chain *i = begin(); i != end(); ++i) 
        *i >> recycler;
      return *this;
    }

    void Wait(bool release_memory = true) {
      threads_.clear();
      for (util::stream::Chain *i = begin(); i != end(); ++i) {
        i->Wait(release_memory);
      }
    }

  private:
    boost::ptr_vector<util::stream::Thread> threads_;

    Chains(const Chains &);
    void operator=(const Chains &);
};

inline void ChainPositions::Init(Chains &chains) {
  FixedArray<util::stream::ChainPosition>::Init(chains.size());
  for (util::stream::Chain *i = chains.begin(); i != chains.end(); ++i) {
    new (end()) util::stream::ChainPosition(i->Add()); Constructed();
  }
}

inline Chains &operator>>(Chains &chains, ChainPositions &positions) {
  positions.Init(chains);
  return chains;
}

class NGramStreams : public FixedArray<NGramStream> {
  public:
    NGramStreams() {}

    // This puts a dummy NGramStream at the beginning (useful to algorithms that need to reference something at the beginning).
    void InitWithDummy(const ChainPositions &positions) {
      FixedArray<NGramStream>::Init(positions.size() + 1);
      new (end()) NGramStream(); Constructed();
      for (const util::stream::ChainPosition *i = positions.begin(); i != positions.end(); ++i) {
        push_back(*i);
      }
    }

    // Limit restricts to positions[0,limit)
    void Init(const ChainPositions &positions, std::size_t limit) {
      FixedArray<NGramStream>::Init(limit);
      for (const util::stream::ChainPosition *i = positions.begin(); i != positions.begin() + limit; ++i) {
        push_back(*i);
      }
    }
    void Init(const ChainPositions &positions) {
      Init(positions, positions.size());
    }

    NGramStreams(const ChainPositions &positions) {
      Init(positions);
    }
};

inline Chains &operator>>(Chains &chains, NGramStreams &streams) {
  ChainPositions positions;
  chains >> positions;
  streams.Init(positions);
  return chains;
}

}} // namespaces
#endif // LM_BUILDER_MULTI_STREAM__

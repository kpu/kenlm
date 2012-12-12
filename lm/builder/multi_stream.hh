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
    FixedArray(const FixedArray &from) {
      std::size_t size = from.newed_end_ - static_cast<const T*>(from.block_.get());
      Init(size);
      for (std::size_t i = 0; i < size; ++i) {
        new(end()) T(from[i]);
        Constructed();
      }
    }

    ~FixedArray() {
      for (T *i = begin(); i != end(); ++i) {
        i->~T();
      }
    }

    T *begin() { return static_cast<T*>(block_.get()); }
    const T *begin() const { return static_cast<const T*>(block_.get()); }
    // Always call Constructed after successful completion of new.  
    T *end() { return newed_end_; }
    const T *end() const { return newed_end_; }

    std::size_t size() const { return end() - begin(); }
    bool empty() const { return begin() == end(); }

    T &operator[](std::size_t i) { return begin()[i]; }
    const T &operator[](std::size_t i) const { return begin()[i]; }

  protected:
    FixedArray() : newed_end_(NULL) {}

    void Init(std::size_t count) {
      assert(!block_.get());
      block_.reset(malloc(sizeof(T) * count));
      if (!block_.get()) throw std::bad_alloc();
      newed_end_ = begin();
    }

    explicit FixedArray(std::size_t count) {
      Init(count);
    }

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
    explicit Chains(std::vector<util::stream::ChainConfig> &config) 
      : FixedArray<util::stream::Chain>(config.size()) {
      for (std::vector<util::stream::ChainConfig>::iterator i = config.begin(); i != config.end(); ++i) {
        i->entry_size = NGram::TotalSize(i - config.begin() + 1);
        new(end()) util::stream::Chain(*i); Constructed();
      }
    }

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

  private:
    boost::ptr_vector<util::stream::Thread> threads_;
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

    void Init(const ChainPositions &positions) {
      FixedArray<NGramStream>::Init(positions.size());
      for (const util::stream::ChainPosition *i = positions.begin(); i != positions.end(); ++i) {
        new (end()) NGramStream(*i);
        Constructed();
      }
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

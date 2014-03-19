#ifndef LM_BUILDER_MULTI_STREAM_H
#define LM_BUILDER_MULTI_STREAM_H

#include "lm/builder/ngram_stream.hh"
#include "util/fixed_array.hh"
#include "util/scoped.hh"
#include "util/stream/chain.hh"

#include <cstddef>
#include <new>

#include <assert.h>
#include <stdlib.h>

namespace lm { namespace builder {

class Chains;

class ChainPositions : public util::FixedArray<util::stream::ChainPosition> {
  public:
    ChainPositions() {}

    void Init(Chains &chains);

    explicit ChainPositions(Chains &chains) {
      Init(chains);
    }
};

class Chains : public util::FixedArray<util::stream::Chain> {
  private:
    template <class T, void (T::*ptr)(const ChainPositions &) = &T::Run> struct CheckForRun {
      typedef Chains type;
    };

  public:
    explicit Chains(std::size_t limit) : util::FixedArray<util::stream::Chain>(limit) {}

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
  util::FixedArray<util::stream::ChainPosition>::Init(chains.size());
  for (util::stream::Chain *i = chains.begin(); i != chains.end(); ++i) {
    new (end()) util::stream::ChainPosition(i->Add()); Constructed();
  }
}

inline Chains &operator>>(Chains &chains, ChainPositions &positions) {
  positions.Init(chains);
  return chains;
}

class NGramStreams : public util::FixedArray<NGramStream> {
  public:
    NGramStreams() {}

    // This puts a dummy NGramStream at the beginning (useful to algorithms that need to reference something at the beginning).
    void InitWithDummy(const ChainPositions &positions) {
      util::FixedArray<NGramStream>::Init(positions.size() + 1);
      new (end()) NGramStream(); Constructed();
      for (const util::stream::ChainPosition *i = positions.begin(); i != positions.end(); ++i) {
        push_back(*i);
      }
    }

    // Limit restricts to positions[0,limit)
    void Init(const ChainPositions &positions, std::size_t limit) {
      util::FixedArray<NGramStream>::Init(limit);
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
#endif // LM_BUILDER_MULTI_STREAM_H

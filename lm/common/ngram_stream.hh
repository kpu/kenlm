#ifndef LM_BUILDER_NGRAM_STREAM_H
#define LM_BUILDER_NGRAM_STREAM_H

#include "lm/common/ngram.hh"
#include "util/stream/chain.hh"
#include "util/stream/multi_stream.hh"
#include "util/stream/stream.hh"

#include <cstddef>

namespace lm {

template <class Payload> class NGramStream {
  public:
    NGramStream() : gram_(NULL, 0) {}

    NGramStream(const util::stream::ChainPosition &position) : gram_(NULL, 0) {
      Init(position);
    }

    void Init(const util::stream::ChainPosition &position) {
      stream_.Init(position);
      gram_ = NGram<Payload>(stream_.Get(), NGram<Payload>::OrderFromSize(position.GetChain().EntrySize()));
    }

    NGram<Payload> &operator*() { return gram_; }
    const NGram<Payload> &operator*() const { return gram_; }

    NGram<Payload> *operator->() { return &gram_; }
    const NGram<Payload> *operator->() const { return &gram_; }

    void *Get() { return stream_.Get(); }
    const void *Get() const { return stream_.Get(); }

    operator bool() const { return stream_; }
    bool operator!() const { return !stream_; }
    void Poison() { stream_.Poison(); }

    NGramStream &operator++() {
      ++stream_;
      gram_.ReBase(stream_.Get());
      return *this;
    }

  private:
    NGram<Payload> gram_;
    util::stream::Stream stream_;
};

template <class Payload> inline util::stream::Chain &operator>>(util::stream::Chain &chain, NGramStream<Payload> &str) {
  str.Init(chain.Add());
  return chain;
}

template <class Payload> class NGramStreams : public util::stream::GenericStreams<NGramStream<Payload> > {
  private:
    typedef util::stream::GenericStreams<NGramStream<Payload> > P;
  public:
    NGramStreams() : P() {}
    NGramStreams(const util::stream::ChainPositions &positions) : P(positions) {}
};

} // namespace
#endif // LM_BUILDER_NGRAM_STREAM_H

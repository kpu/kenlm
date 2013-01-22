#ifndef LM_BUILDER_NGRAM_STREAM__
#define LM_BUILDER_NGRAM_STREAM__

#include "lm/builder/ngram.hh"
#include "util/stream/chain.hh"
#include "util/stream/stream.hh"

#include <cstddef>

namespace lm { namespace builder {

class NGramStream {
  public:
    NGramStream() : gram_(NULL, 0) {}

    NGramStream(const util::stream::ChainPosition &position) : gram_(NULL, 0) {
      Init(position);
    }

    void Init(const util::stream::ChainPosition &position) {
      stream_.Init(position);
      gram_ = NGram(stream_.Get(), NGram::OrderFromSize(position.GetChain().EntrySize()));
    }

    NGram &operator*() { return gram_; }
    const NGram &operator*() const { return gram_; }

    NGram *operator->() { return &gram_; }
    const NGram *operator->() const { return &gram_; }

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
    NGram gram_;
    util::stream::Stream stream_;
};

inline util::stream::Chain &operator>>(util::stream::Chain &chain, NGramStream &str) {
  str.Init(chain.Add());
  return chain;
}

}} // namespaces
#endif // LM_BUILDER_NGRAM_STREAM__

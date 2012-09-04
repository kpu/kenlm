#ifndef BUILDER_NGRAM__
#define BUILDER_NGRAM__

#include "lm/word_index.hh"

namespace lm {
namespace builder {

template <unsigned N> struct NGram {
  static const unsigned n = N;
  WordIndex w[N];
};

template <unsigned N> struct CountedNGram : NGram<N> {
  uint64_t count;
};

} // namespace builder
} // namespace lm

#endif // BUILDER_NGRAM__

#ifdef BUILDER_NGRAM__
#define BUILDER_NGRAM__

#include "lm/word_index.hh"

namespace builder {

template <unsigned N> struct NGram {
  static const unsigned n = N;
  lm::WordIndex w[N];
};

template <unsigned N> struct CountedNGram : NGram<N> {
  uint64_t count;
};

} // namespace builder

#endif // BUILDER_NGRAM__

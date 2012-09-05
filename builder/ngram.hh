#ifndef BUILDER_NGRAM__
#define BUILDER_NGRAM__

#include "lm/word_index.hh"

#include <inttypes.h>
#include <string.h>

namespace lm {
namespace builder {

template <unsigned N> struct NGram {
  static const unsigned n = N;
  WordIndex w[N];
  bool operator==(const NGram<N> &gram) const {
    return !memcmp(w, gram.w, N * sizeof(WordIndex));
  }
};

template <unsigned N> struct CountedNGram : NGram<N> {
  uint64_t count;
};

const WordIndex kBOS = 1;

} // namespace builder
} // namespace lm

#endif // BUILDER_NGRAM__

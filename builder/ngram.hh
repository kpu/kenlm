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
    return !memcmp(w, gram.w, sizeof(w));
  }
};

template <unsigned N> struct CountedNGram : NGram<N> {
  uint64_t count;
};

template <unsigned N> struct UninterpNGram : NGram<N> {
  float prob;  // Uninterpolated probability.
  float gamma; // Interpolation weight for lower order.
};

template <unsigned N> struct InterpNGram : NGram<N> {
  float prob;  // p(w_n | w_1^{n-1})
  float lower; // p(w_n | w_2^{n-1})
};

const WordIndex kBOS = 1;

#define STATICALLY_DISPATCH(i) do { \
  switch (i) { \
    case 1: { XX(1); } break; \
    case 2: { XX(2); } break; \
    case 3: { XX(3); } break; \
    case 4: { XX(4); } break; \
    case 5: { XX(5); } break; \
    default: \
      fprintf(stderr, "%d-grams are not supported; please, accomodate ngram.hh to your needs\n", i); \
      break; \
  } \
} while(0)

} // namespace builder
} // namespace lm

#endif // BUILDER_NGRAM__

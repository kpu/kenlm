#ifdef BUILDER_NGRAM__
#define BUILDER_NGRAM__

#include "lm/word_index.hh"

namespace builder {

template <unsigned N> struct NGram {
  lm::WordIndex w[N];
};

} // namespace builder

#endif // BUILDER_NGRAM__

#include "lm/builder/interpolate.hh"

#include "lm/builder/multi_stream.hh"
#include "lm/lm_exception.hh"

#include <assert.h>

namespace lm { namespace builder {

namespace {
bool Matched(const NGramStreams &streams, unsigned length) {
  return !memcmp(streams[length - 1]->begin(), streams[length]->begin() + 1, sizeof(WordIndex) * length);
}
} // namespace

void Interpolate::Run(const ChainPositions &positions) {
  NGramStreams streams(positions);
  std::vector<float> probs(streams.size() + 1);
  probs[0] = 1.0 / static_cast<float>(unigram_count_ - 1); // exclude <s> from unigram count
  // The effective order.  
  unsigned int order;
  for (order = 0; order < streams.size() && streams[order]; ++order) {}
  assert(order); // should always have <unk>.
  unsigned int current = 0;
  while (true) {
    float lower = probs[current];
    Payload &pay = streams[current]->Value();
    // Ordering is important so as to not overwrite gamma.  
    pay.interp.prob = pay.uninterp.prob + pay.uninterp.gamma * lower;
    pay.interp.lower = lower;
    if (current + 1 < order && Matched(streams, current + 1)) {
      // Transition to higher order.  
      probs[++current] = pay.interp.prob;
    } else {
      ++streams[current];
      if (!streams[current]) {
        UTIL_THROW_IF(order != current + 1, FormatLoadException, "Detected n-gram without matching suffix");
        order = current;
        if (!order) return;
        --current;
      } else if (current && Matched(streams, current)) {
        --current;
      }
    }
  }
}

}} // namespaces

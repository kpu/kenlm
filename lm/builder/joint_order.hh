#ifndef LM_BUILDER_JOINT_ORDER__
#define LM_BUILDER_JOINT_ORDER__

#include "lm/builder/multi_stream.hh"
#include "lm/lm_exception.hh"

#include <string.h>

namespace lm { namespace builder {

template <class Callback, class Compare> void JointOrder(const ChainPositions &positions, Callback &callback) {
  // Allow matching to reference streams[-1].
  NGramStreams streams_with_dummy;
  streams_with_dummy.InitWithDummy(positions);
  NGramStream *streams = streams_with_dummy.begin() + 1;

  unsigned int order;
  for (order = 0; order < positions.size() && streams[order]; ++order) {}
  assert(order); // should always have <unk>.
  unsigned int current = 0;
  while (true) {
    // Does the context match the lower one?  
    if (!memcmp(streams[static_cast<int>(current) - 1]->begin(), streams[current]->begin() + Compare::kMatchOffset, sizeof(WordIndex) * current)) {
      callback.Enter(current, *streams[current]);
      // Transition to looking for extensions.  
      if (++current < order) continue;
    }
    // No extension left.  
    while(true) {
      assert(current > 0);
      --current;
      callback.Exit(current, *streams[current]);
      if (++streams[current]) break;
      UTIL_THROW_IF(order != current + 1, FormatLoadException, "Detected n-gram without matching suffix");
      order = current;
      if (!order) return;
    }
  }
}

}} // namespaces

#endif // LM_BUILDER_JOINT_ORDER__

#ifndef LM_BUILDER_JOINT_ORDER_H
#define LM_BUILDER_JOINT_ORDER_H

#include "lm/builder/ngram_stream.hh"
#include "lm/lm_exception.hh"

#ifdef DEBUG
#include "util/fixed_array.hh"
#include <iostream>
#endif

#include <string.h>

namespace lm { namespace builder {

template <class Callback, class Compare> void JointOrder(const util::stream::ChainPositions &positions, Callback &callback) {
  // Allow matching to reference streams[-1].
  NGramStreams streams_with_dummy;
  streams_with_dummy.InitWithDummy(positions);
  NGramStream *streams = streams_with_dummy.begin() + 1;

  unsigned int order;
  for (order = 0; order < positions.size() && streams[order]; ++order) {}
  assert(order); // should always have <unk>.

  // Debugging only: call comparison function to sanity check order.
#ifdef DEBUG
  util::FixedArray<Compare> less_compare(order);
  for (unsigned i = 0; i < order; ++i)
    less_compare.push_back(i + 1);
#endif // DEBUG

  unsigned int current = 0;
  while (true) {
    // Does the context match the lower one?
    if (!memcmp(streams[static_cast<int>(current) - 1]->begin(), streams[current]->begin() + Compare::kMatchOffset, sizeof(WordIndex) * current)) {
      callback.Enter(current, *streams[current]);
      // Transition to looking for extensions.  
      if (++current < order) continue;
    }
#ifdef DEBUG
    // match_check[current - 1] matches current-grams
    // The lower-order stream (which skips fewer current-grams) should always be <= the higher order-stream (which can skip current-grams).
    else if (!less_compare[current - 1](streams[static_cast<int>(current) - 1]->begin(), streams[current]->begin() + Compare::kMatchOffset)) {
      std::cerr << "Stream out of order detected" << std::endl;
      abort();
    }
#endif // DEBUG
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

#endif // LM_BUILDER_JOINT_ORDER_H

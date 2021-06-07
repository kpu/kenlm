#ifndef LM_BUILDER_COMBINE_COUNTS_H
#define LM_BUILDER_COMBINE_COUNTS_H

#include "payload.hh"
#include "../common/ngram.hh"
#include "../common/compare.hh"
#include "../word_index.hh"
#include "../../util/stream/sort.hh"

#include <functional>
#include <string>

namespace lm {
namespace builder {

// Sum counts for the same n-gram.
struct CombineCounts {
  bool operator()(void *first_void, const void *second_void, const SuffixOrder &compare) const {
    NGram<BuildingPayload> first(first_void, compare.Order());
    // There isn't a const version of NGram.
    NGram<BuildingPayload> second(const_cast<void*>(second_void), compare.Order());
    if (memcmp(first.begin(), second.begin(), sizeof(WordIndex) * compare.Order())) return false;
    first.Value().count += second.Value().count;
    return true;
  }
};

} // namespace builder
} // namespace lm

#endif // LM_BUILDER_COMBINE_COUNTS_H

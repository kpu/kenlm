#include "lm/builder/adjust_counts.hh"

#include "lm/builder/multi_stream.hh"

#include <algorithm>

namespace lm { namespace builder {
namespace {
// Return last word in full that is different.  
const WordIndex* FindDifference(NGramStream &full, NGramStream &lower_last) {
  const WordIndex *cur_word = full->end() - 1;
  WordIndex *pre_word = lower_last->end() - 1;
  // Find last difference.  
  for (; pre_word >= lower_last->begin() && *pre_word == *cur_word; --cur_word, --pre_word) {}
  return cur_word;
}
} // namespace

// TODO: <s> including deletion
void AdjustCounts(const ChainPositions &positions) {
  NGramStreams streams(positions);
  const std::size_t order = streams.size();
  if (order == 1) return; // Nothing to do if it's only unigrams.  
  NGramStream *const lower_end = streams.end() - 1;
  NGramStream &full = *(lower_end);

  if (!full) return; // Odd, no N-gram at all.  

  // Initialization: unigrams are valid.  
  NGramStream *lower_valid = streams.begin();
  (*lower_valid)->Count() = 0;
  *(*lower_valid)->begin() = *(full->end() - 1);

  for (; full; ++full) {
    const WordIndex *different = FindDifference(full, *lower_valid);
    std::size_t same = full->end() - 1 - different;
    // Increment the adjusted count.  
    if (same) ++streams[same - 1]->Count();

    // Output all the valid ones that changed.  
    for (; lower_valid >= &streams[same]; --lower_valid) {
      ++*lower_valid;
    }

    // This is here because bos is also const WordIndex *, so copy gets
    // consistent argument types.  
    const WordIndex *full_end = full->end();
    // Initialize and mark as valid up to bos.  
    const WordIndex *bos;
    for (bos = different; (bos > full->begin()) && (*bos != kBOS); --bos) {
      ++lower_valid;
      std::copy(bos, full_end, (*lower_valid)->begin());
      (*lower_valid)->Count() = 1;
    }
    // Now bos indicates where <s> is or is the 0th word of full.  
    if (bos != full->begin()) {
      // There is an <s> beyond the 0th word.  
      NGramStream &to = *++lower_valid;
      std::copy(bos, full_end, to->begin());
      to->Count() = full->Count();
      
      // Make a tombstone in the N-grams.   
      std::fill(full->begin(), full->end(), kTombstone);
      full->Count() = 0;
    }
    assert(lower_valid >= &streams[0]);
  }

  // Output everything valid.
  for (NGramStream *s = streams.begin(); s <= lower_valid; ++s) {
    ++*s;
  }
  // Poison everyone!  Except the N-grams which were already poisoned by the input.   
  for (NGramStream *s = streams.begin(); s != streams.end() - 1; ++s) {
    s->Poison();
  }
}

}} // namespaces

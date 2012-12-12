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

class StatCollector {
  public:
    StatCollector(std::size_t order) : orders_(order), full_(orders_.back()) {
      memset(&orders_[0], 0, sizeof(OrderStat) * order);
    }

    void Add(std::size_t order_minus_1, uint64_t count) {
      OrderStat &stat = orders_[order_minus_1];
      ++stat.count;
      if (count < 5) ++stat.n[count - 1];
    }

    void AddFull(uint64_t count) {
      ++full_.count;
      if (count < 5) ++full_.n[count - 1];
    }

    void Complete(std::vector<uint64_t> &counts, std::vector<Discount> &discounts) const {
      counts.resize(orders_.size());
      discounts.resize(orders_.size());
      for (std::size_t i = 0; i < orders_.size(); ++i) {
        const OrderStat &s = orders_[i];
        counts[i] = s.count;
        // See equation (26) in Chen and Goodman.
        discounts[i].amount[0] = 0.0;
        float y = static_cast<float>(s.n[0]) / static_cast<float>(s.n[0] + 2.0 * s.n[1]);
        for (unsigned j = 1; j < 4; ++j) {
          discounts[i].amount[j] = static_cast<float>(i) - static_cast<float>(i + 1) * y * static_cast<float>(s.n[j]) / static_cast<float>(s.n[j-1]);
        }
      }
    }

  private:
    struct OrderStat {
      // n_[0] is n_1 in equation 26 of Chen and Goodman
      uint64_t n[4];
      uint64_t count;
    };

    std::vector<OrderStat> orders_;
    OrderStat &full_;
};

} // namespace

void AdjustCounts::Run(const ChainPositions &positions) {
  NGramStreams streams(positions);
  NGramStream *const lower_end = streams.end() - 1;
  NGramStream &full = *(lower_end);

  const std::size_t order = streams.size();
  StatCollector stats(order);
  if (order == 1) {
    // Only unigrams.  Just collect stats.  
    for (; full; ++full) 
      stats.AddFull(full->Count());
    stats.Complete(counts_, discounts_);
    return;
  }
  if (!full) {
    // No n-gram at all, oddly.
    stats.Complete(counts_, discounts_);
    return;
  }

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
      stats.Add(lower_valid - streams.begin(), (*lower_valid)->Count());
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
    } else {
      stats.AddFull(full->Count());
    }
    assert(lower_valid >= &streams[0]);
  }

  // Output everything valid.
  for (NGramStream *s = streams.begin(); s <= lower_valid; ++s) {
    stats.Add(s - streams.begin(), (*s)->Count());
    ++*s;
  }
  // Poison everyone!  Except the N-grams which were already poisoned by the input.   
  for (NGramStream *s = streams.begin(); s != streams.end() - 1; ++s) {
    s->Poison();
  }
  stats.Complete(counts_, discounts_);
}

}} // namespaces

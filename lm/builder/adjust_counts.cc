#include "lm/builder/adjust_counts.hh"
#include "lm/builder/ngram_stream.hh"
#include "util/stream/timer.hh"

#include <algorithm>
#include <iostream>

namespace lm { namespace builder {

BadDiscountException::BadDiscountException() throw() {}
BadDiscountException::~BadDiscountException() throw() {}

namespace {
// Return last word in full that is different.
const WordIndex* FindDifference(const NGram &full, const NGram &lower_last) {
  const WordIndex *cur_word = full.end() - 1;
  const WordIndex *pre_word = lower_last.end() - 1;
  // Find last difference.
  for (; pre_word >= lower_last.begin() && *pre_word == *cur_word; --cur_word, --pre_word) {}
  return cur_word;
}

class StatCollector {
  public:
    StatCollector(std::size_t order, std::vector<uint64_t> &counts, std::vector<uint64_t> &counts_pruned, std::vector<Discount> &discounts)
      : orders_(order), full_(orders_.back()), counts_(counts), counts_pruned_(counts_pruned), discounts_(discounts) {
      memset(&orders_[0], 0, sizeof(OrderStat) * order);
    }

    ~StatCollector() {}

    void CalculateDiscounts() {
      counts_.resize(orders_.size());
      counts_pruned_.resize(orders_.size());
      discounts_.resize(orders_.size());
      for (std::size_t i = 0; i < orders_.size(); ++i) {
        const OrderStat &s = orders_[i];
        counts_[i] = s.count;
        counts_pruned_[i] = s.count_pruned;

        for (unsigned j = 1; j < 4; ++j) {
          // TODO: Specialize error message for j == 3, meaning 3+
          UTIL_THROW_IF(s.n[j] == 0, BadDiscountException, "Could not calculate Kneser-Ney discounts for "
              << (i+1) << "-grams with adjusted count " << (j+1) << " because we didn't observe any "
              << (i+1) << "-grams with adjusted count " << j << "; Is this small or artificial data?");
        }

        // See equation (26) in Chen and Goodman.
        discounts_[i].amount[0] = 0.0;
        float y = static_cast<float>(s.n[1]) / static_cast<float>(s.n[1] + 2.0 * s.n[2]);
        for (unsigned j = 1; j < 4; ++j) {
          discounts_[i].amount[j] = static_cast<float>(j) - static_cast<float>(j + 1) * y * static_cast<float>(s.n[j+1]) / static_cast<float>(s.n[j]);
          UTIL_THROW_IF(discounts_[i].amount[j] < 0.0 || discounts_[i].amount[j] > j, BadDiscountException, "ERROR: " << (i+1) << "-gram discount out of range for adjusted count " << j << ": " << discounts_[i].amount[j]);
        }
      }
    }

    void Add(std::size_t order_minus_1, uint64_t count, bool pruned = false) {
      OrderStat &stat = orders_[order_minus_1];
      ++stat.count;
      if (!pruned)
        ++stat.count_pruned;
      if (count < 5) ++stat.n[count];
    }

    void AddFull(uint64_t count, bool pruned = false) {
      ++full_.count;
      if (!pruned)
        ++full_.count_pruned;
      if (count < 5) ++full_.n[count];
    }

  private:
    struct OrderStat {
      // n_1 in equation 26 of Chen and Goodman etc
      uint64_t n[5];
      uint64_t count;
      uint64_t count_pruned;
    };

    std::vector<OrderStat> orders_;
    OrderStat &full_;

    std::vector<uint64_t> &counts_;
    std::vector<uint64_t> &counts_pruned_;
    std::vector<Discount> &discounts_;
};

// Reads all entries in order like NGramStream does.
// But deletes any entries that have <s> in the 1st (not 0th) position on the
// way out by putting other entries in their place.  This disrupts the sort
// order but we don't care because the data is going to be sorted again.
class CollapseStream {
  public:
    CollapseStream(const util::stream::ChainPosition &position, uint64_t prune_threshold) :
      current_(NULL, NGram::OrderFromSize(position.GetChain().EntrySize())),
      prune_threshold_(prune_threshold),
      block_(position) { 
      StartBlock();
    }

    const NGram &operator*() const { return current_; }
    const NGram *operator->() const { return &current_; }

    operator bool() const { return block_; }

    CollapseStream &operator++() {
      assert(block_);
      
      if (current_.begin()[1] == kBOS && current_.Base() < copy_from_) {
        memcpy(current_.Base(), copy_from_, current_.TotalSize());
        UpdateCopyFrom();
        
        // Mark highest order n-grams for later pruning
        if(current_.Count() <= prune_threshold_) {
          current_.Mark(); 
        }
        
      }
    
      current_.NextInMemory();
      uint8_t *block_base = static_cast<uint8_t*>(block_->Get());
      if (current_.Base() == block_base + block_->ValidSize()) {
        block_->SetValidSize(copy_from_ + current_.TotalSize() - block_base);
        ++block_;
        StartBlock();
      }
      
      // Mark highest order n-grams for later pruning
      if(current_.Count() <= prune_threshold_) {
        current_.Mark(); 
      }
      
      return *this;
    }

  private:
    void StartBlock() {
      for (; ; ++block_) {
        if (!block_) return;
        if (block_->ValidSize()) break;
      }
      current_.ReBase(block_->Get());
      copy_from_ = static_cast<uint8_t*>(block_->Get()) + block_->ValidSize();
      UpdateCopyFrom();
      
      // Mark highest order n-grams for later pruning
      if(current_.Count() <= prune_threshold_) {
        current_.Mark(); 
      }
      
    }

    // Find last without bos.
    void UpdateCopyFrom() {
      for (copy_from_ -= current_.TotalSize(); copy_from_ >= current_.Base(); copy_from_ -= current_.TotalSize()) {
        if (NGram(copy_from_, current_.Order()).begin()[1] != kBOS) break;
      }
    }

    NGram current_;

    // Goes backwards in the block
    uint8_t *copy_from_;
    uint64_t prune_threshold_;
    util::stream::Link block_;
};

} // namespace

void AdjustCounts::Run(const util::stream::ChainPositions &positions) {
  UTIL_TIMER("(%w s) Adjusted counts\n");

  const std::size_t order = positions.size();
  StatCollector stats(order, counts_, counts_pruned_, discounts_);
  if (order == 1) {

    // Only unigrams.  Just collect stats.  
    for (NGramStream full(positions[0]); full; ++full) 
      stats.AddFull(full->Count());

    stats.CalculateDiscounts();
    return;
  }

  NGramStreams streams;
  streams.Init(positions, positions.size() - 1);
  
  CollapseStream full(positions[positions.size() - 1], prune_thresholds_.back());

  // Initialization: <unk> has count 0 and so does <s>.
  NGramStream *lower_valid = streams.begin();
  streams[0]->Count() = 0;
  *streams[0]->begin() = kUNK;
  stats.Add(0, 0);
  (++streams[0])->Count() = 0;
  *streams[0]->begin() = kBOS;
  // not in stats because it will get put in later.

  std::vector<uint64_t> lower_counts(positions.size(), 0);
  
  // iterate over full (the stream of the highest order ngrams)
  for (; full; ++full) {  
    const WordIndex *different = FindDifference(*full, **lower_valid);
    std::size_t same = full->end() - 1 - different;
    // Increment the adjusted count.
    if (same) ++streams[same - 1]->Count();

    // Output all the valid ones that changed.
    for (; lower_valid >= &streams[same]; --lower_valid) {
      
      // mjd: review this!
      uint64_t order = (*lower_valid)->Order();
      uint64_t realCount = lower_counts[order - 1];
      if(order > 1 && prune_thresholds_[order - 1] && realCount <= prune_thresholds_[order - 1])
        (*lower_valid)->Mark();
      
      stats.Add(lower_valid - streams.begin(), (*lower_valid)->UnmarkedCount(), (*lower_valid)->IsMarked());
      ++*lower_valid;
    }
    
    // Count the true occurrences of lower-order n-grams
    for (std::size_t i = 0; i < lower_counts.size(); ++i) {
        if (i >= same) {
          lower_counts[i] = 0;
        }
        lower_counts[i] += full->UnmarkedCount();
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

      // mjd: what is this doing?
      to->Count() = full->UnmarkedCount(); 
    } else {
      stats.AddFull(full->UnmarkedCount(), full->IsMarked()); 
    }
    assert(lower_valid >= &streams[0]);
  }

  // Output everything valid.
  for (NGramStream *s = streams.begin(); s <= lower_valid; ++s) {
    if((*s)->Count() <= prune_thresholds_[(*s)->Order() - 1])
      (*s)->Mark();
    stats.Add(s - streams.begin(), (*s)->UnmarkedCount(), (*s)->IsMarked());
    ++*s;
  }
  // Poison everyone!  Except the N-grams which were already poisoned by the input.
  for (NGramStream *s = streams.begin(); s != streams.end(); ++s)
    s->Poison();

  stats.CalculateDiscounts();

  // NOTE: See special early-return case for unigrams near the top of this function
}

}} // namespaces

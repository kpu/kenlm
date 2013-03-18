#ifndef LM_TRIE__
#define LM_TRIE__

#include "lm/weights.hh"
#include "lm/word_index.hh"
#include "util/bit_packing.hh"
#include "util/mmap.hh"

#include <cstddef>

#include <stdint.h>

namespace lm {
namespace ngram {
struct Config;
namespace trie {

struct NodeRange {
  uint64_t begin, end;
};

// TODO: if the number of unigrams is a concern, also bit pack these records.  
struct UnigramValue {
  ProbBackoff weights;
  uint64_t next;
  uint64_t Next() const { return next; }
};

class UnigramPointer {
  public:
    explicit UnigramPointer(const ProbBackoff &to) : to_(&to) {}

    UnigramPointer() : to_(NULL) {}

    bool Found() const { return to_ != NULL; }

    float Prob() const { return to_->prob; }
    float Backoff() const { return to_->backoff; }
    float Rest() const { return Prob(); }

  private:
    const ProbBackoff *to_;
};

class Unigram {
  public:
    Unigram() {}
    
    void Init(const util::Rolling &mem) {
      base_ = mem;
    }
    
    static uint64_t Size(uint64_t count) {
      // +1 in case unknown doesn't appear.  +1 for the final next.  
      return (count + 2) * sizeof(UnigramValue);
    }
    
    const ProbBackoff &Lookup(WordIndex word) const {
      return UncheckedIndex(word).weights;
    }
    
    ProbBackoff &Unknown() {
      return CheckedIndex(0).weights;
    }

    UnigramValue *Raw() {
      return static_cast<UnigramValue*>(base_.get());
    }

    UnigramValue &CheckedIndex(WordIndex word) {
      return *static_cast<UnigramValue*>(base_.CheckedIndex(sizeof(UnigramValue) * word));
    }
    
    void LoadedBinary() {}

    UnigramPointer Find(WordIndex word, NodeRange &next) const {
      const UnigramValue *val = &UncheckedIndex(word);
      next.begin = val->next;
      next.end = (val+1)->next;
      assert(next.end >= next.begin);
      return UnigramPointer(val->weights);
    }

  private:
    const UnigramValue &UncheckedIndex(WordIndex word) const {
      return *(static_cast<const UnigramValue*>(base_.get()) + word);
    }

    util::Rolling base_;
};  

class BitPacked {
  public:
    BitPacked() {}

    uint64_t InsertIndex() const {
      return insert_index_;
    }

  protected:
    static uint64_t BaseSize(uint64_t entries, uint64_t max_vocab, uint8_t remaining_bits);

    void BaseInit(uint64_t max_vocab, uint8_t remaining_bits);

    uint8_t word_bits_;
    uint8_t total_bits_;
    uint64_t word_mask_;

    util::Rolling base_;

    uint64_t insert_index_, max_vocab_;
};

template <class Bhiksha> class BitPackedMiddle : public BitPacked {
  public:
    static uint64_t Size(uint8_t quant_bits, uint64_t entries, uint64_t max_vocab, uint64_t max_next, const Config &config);

    // next_source need not be initialized.  
    BitPackedMiddle(const util::Rolling &mem, uint8_t quant_bits, uint64_t entries, uint64_t max_vocab, uint64_t max_next, const BitPacked &next_source, const Config &config);

    util::BitAddress Insert(WordIndex word);

    void FinishedLoading(uint64_t next_end, const Config &config);

    void LoadedBinary() { bhiksha_.LoadedBinary(); }

    util::BitAddress Find(WordIndex word, NodeRange &range, uint64_t &pointer) const;

    util::BitAddress ReadEntry(uint64_t pointer, NodeRange &range) {
      uint64_t addr = pointer * total_bits_;
      addr += word_bits_;
      bhiksha_.ReadNext(base_.get(), addr + quant_bits_, pointer, total_bits_, range);
      return util::BitAddress(base_.get(), addr);
    }

  private:
    uint8_t quant_bits_;
    Bhiksha bhiksha_;

    const BitPacked *next_source_;
};

class BitPackedLongest : public BitPacked {
  public:
    static uint64_t Size(uint8_t quant_bits, uint64_t entries, uint64_t max_vocab) {
      return BaseSize(entries, max_vocab, quant_bits);
    }

    BitPackedLongest() {}

    void Init(const util::Rolling &mem, uint8_t quant_bits, uint64_t max_vocab) {
      base_ = mem;
      BaseInit(max_vocab, quant_bits);
    }

    void LoadedBinary() {}

    util::BitAddress Insert(WordIndex word);

    util::BitAddress Find(WordIndex word, const NodeRange &node) const;

  private:
    uint8_t quant_bits_;
};

} // namespace trie
} // namespace ngram
} // namespace lm

#endif // LM_TRIE__

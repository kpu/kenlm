#ifndef LM_TRIE__
#define LM_TRIE__

#include <stdint.h>

#include <cstddef>

#include "lm/word_index.hh"
#include "lm/weights.hh"

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

class Unigram {
  public:
    Unigram() {}
    
    void Init(void *start) {
      unigram_ = static_cast<UnigramValue*>(start);
    }
    
    static std::size_t Size(uint64_t count) {
      // +1 in case unknown doesn't appear.  +1 for the final next.  
      return (count + 2) * sizeof(UnigramValue);
    }
    
    const ProbBackoff &Lookup(WordIndex index) const { return unigram_[index].weights; }
    
    ProbBackoff &Unknown() { return unigram_[0].weights; }

    UnigramValue *Raw() {
      return unigram_;
    }
    
    void LoadedBinary() {}

    void Find(WordIndex word, float &prob, float &backoff, NodeRange &next) const {
      UnigramValue *val = unigram_ + word;
      prob = val->weights.prob;
      backoff = val->weights.backoff;
      next.begin = val->next;
      next.end = (val+1)->next;
    }

  private:
    UnigramValue *unigram_;
};  

class BitPacked {
  public:
    BitPacked() {}

    uint64_t InsertIndex() const {
      return insert_index_;
    }

  protected:
    static std::size_t BaseSize(uint64_t entries, uint64_t max_vocab, uint8_t remaining_bits);

    void BaseInit(void *base, uint64_t max_vocab, uint8_t remaining_bits);

    uint8_t word_bits_;
    uint8_t total_bits_;
    uint64_t word_mask_;

    uint8_t *base_;

    uint64_t insert_index_, max_vocab_;
};

template <class Quant, class Bhiksha> class BitPackedMiddle : public BitPacked {
  public:
    static std::size_t Size(uint8_t quant_bits, uint64_t entries, uint64_t max_vocab, uint64_t max_next, const Config &config);

    // next_source need not be initialized.  
    BitPackedMiddle(void *base, const Quant &quant, uint64_t entries, uint64_t max_vocab, uint64_t max_next, const BitPacked &next_source, const Config &config);

    void Insert(WordIndex word, float prob, float backoff);

    void FinishedLoading(uint64_t next_end, const Config &config);

    void LoadedBinary() { bhiksha_.LoadedBinary(); }

    bool Find(WordIndex word, float &prob, float &backoff, NodeRange &range, uint64_t &pointer) const;

    bool FindNoProb(WordIndex word, float &backoff, NodeRange &range) const;

    NodeRange ReadEntry(uint64_t pointer, float &prob) {
      uint64_t addr = pointer * total_bits_;
      addr += word_bits_;
      quant_.ReadProb(base_, addr, prob);
      NodeRange ret;
      bhiksha_.ReadNext(base_, addr + quant_.TotalBits(), pointer, total_bits_, ret);
      return ret;
    }

  private:
    Quant quant_;
    Bhiksha bhiksha_;

    const BitPacked *next_source_;
};

template <class Quant> class BitPackedLongest : public BitPacked {
  public:
    static std::size_t Size(uint8_t quant_bits, uint64_t entries, uint64_t max_vocab) {
      return BaseSize(entries, max_vocab, quant_bits);
    }

    BitPackedLongest() {}

    void Init(void *base, const Quant &quant, uint64_t max_vocab) {
      quant_ = quant;
      BaseInit(base, max_vocab, quant_.TotalBits());
    }

    void LoadedBinary() {}

    void Insert(WordIndex word, float prob);

    bool Find(WordIndex word, float &prob, const NodeRange &node) const;

  private:
    Quant quant_;
};

} // namespace trie
} // namespace ngram
} // namespace lm

#endif // LM_TRIE__

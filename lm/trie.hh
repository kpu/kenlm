#ifndef LM_TRIE__
#define LM_TRIE__

#include <inttypes.h>

#include <cstddef>

#include "lm/word_index.hh"
#include "lm/weights.hh"

namespace lm {
namespace ngram {
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

    bool Find(WordIndex word, float &prob, float &backoff, NodeRange &next) const {
      UnigramValue *val = unigram_ + word;
      prob = val->weights.prob;
      backoff = val->weights.backoff;
      next.begin = val->next;
      next.end = (val+1)->next;
      return true;
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

    void LoadedBinary() {}

  protected:
    static std::size_t BaseSize(uint64_t entries, uint64_t max_vocab, uint8_t remaining_bits);

    void BaseInit(void *base, uint64_t max_vocab, uint8_t remaining_bits);

    uint8_t word_bits_, prob_bits_;
    uint8_t total_bits_;
    uint64_t word_mask_;

    uint8_t *base_;

    uint64_t insert_index_;
};

class BitPackedMiddle : public BitPacked {
  public:
    BitPackedMiddle() {}

    static std::size_t Size(uint64_t entries, uint64_t max_vocab, uint64_t max_next);

    // next_source need not be initialized.  
    void Init(void *base, uint64_t max_vocab, uint64_t max_next, const BitPacked &next_source);

    void Insert(WordIndex word, float prob, float backoff);

    bool Find(WordIndex word, float &prob, float &backoff, NodeRange &range) const;

    bool FindNoProb(WordIndex word, float &backoff, NodeRange &range) const;

    void FinishedLoading(uint64_t next_end);

  private:
    uint8_t backoff_bits_, next_bits_;
    uint64_t next_mask_;

    const BitPacked *next_source_;
};


class BitPackedLongest : public BitPacked {
  public:
    BitPackedLongest() {}

    static std::size_t Size(uint64_t entries, uint64_t max_vocab) {
      return BaseSize(entries, max_vocab, 0);
    }

    void Init(void *base, uint64_t max_vocab) {
      return BaseInit(base, max_vocab, 0);
    }

    void Insert(WordIndex word, float prob);

    bool Find(WordIndex word, float &prob, const NodeRange &node) const;
};

} // namespace trie
} // namespace ngram
} // namespace lm

#endif // LM_TRIE__

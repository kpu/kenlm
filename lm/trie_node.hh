#ifndef LM_TRIE_NODE__
#define LM_TRIE_NODE__

#include <inttypes.h>

#include <cstddef>

#include "lm/word_index.hh"

namespace lm {
namespace trie {

struct NodeRange {
  uint64_t begin, end;
};

class BitPacked {
  public:
    BitPacked() {}

    uint64_t InsertIndex() const {
      return insert_index_;
    }

  protected:
    static std::size_t BaseSize(std::size_t entries, uint64_t max_vocab, uint8_t remaining_bits);

    void BaseInit(void *base, uint64_t max_vocab, uint8_t remaining_bits);

    uint8_t word_bits_, prob_bits_;
    uint8_t total_bits_;
    uint64_t word_mask_;

    uint8_t *base_;

    uint64_t insert_index_;
};

class BitPackedLongest : public BitPacked {
  public:
    BitPackedLongest() {}

    static std::size_t Size(std::size_t entries, uint64_t max_vocab) {
      return BaseSize(entries, max_vocab, 0);
    }

    void Init(void *base, uint64_t max_vocab) {
      return BaseInit(base, max_vocab, 0);
    }

    void Insert(WordIndex word, float prob);

    bool Find(const NodeRange &range, WordIndex word, float &prob) const;
};

class BitPackedMiddle : public BitPacked {
  public:
    BitPackedMiddle() {}

    static std::size_t Size(std::size_t entries, uint64_t max_vocab, uint64_t max_next);

    void Init(void *base, uint64_t max_vocab, uint64_t max_next);

    void Insert(WordIndex word, float prob, float backoff, uint64_t next);

    bool Find(const NodeRange &range, WordIndex word, float &prob, float &backoff, NodeRange &next_range) const;

    void Finish(uint64_t next_end);

  private:
    uint8_t backoff_bits_, next_bits_;
    uint64_t next_mask_;
};

} // namespace trie
} // namespace lm

#endif // LM_TRIE_NODE__

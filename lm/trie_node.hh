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

class BitPackedTable {
  public:
    BitPackedTable() {}

    static std::size_t Size(std::size_t entries, bool is_max_order, uint64_t max_vocab, uint64_t max_ptr);

    void Init(void *base, bool is_max_order, uint64_t max_vocab, uint64_t max_ptr);

    bool FindWithNext(const NodeRange &pointer, WordIndex index, float &prob, float &backoff, NodeRange &next_pointer) const;

    uint64_t InsertPointer() const {
      return insert_pointer_;
    }

    void Insert(WordIndex index, float prob, float backoff, uint64_t pointer);

    void Finish(uint64_t end_pointer);

  private:
    uint8_t index_bits_, prob_bits_, backoff_bits_, ptr_bits_;
    uint8_t total_bits_;
    uint64_t index_mask_, ptr_mask_;

    uint8_t *base_;

    uint64_t insert_pointer_;
};

/*template <class Value> class SimpleTrie {
  private:
    struct Entry {
      WordIndex key;
      WordIndex GetKey() const { return key; }
      Value value;
    };

  public:
    SimpleTrie() {}

    void Init(void *mem, std::size_t entries) {
      begin_ = reinterpret_cast<const Entry*>(mem);
      end_ = reinterpret_cast<Entry*>(mem);
      total_entries_ = entries;
    }

    bool FindWithNext(uint64_t begin, uint64_t end, WordIndex key, Value &out, uint64_t &next_begin, uint64_t &next_end) const {
      const Entry *found;
      if (!util::SortedUniformFind(begin_ + begin, begin_ + end, key, found)) return false;
      out = found->value;
      delta_out = (found+1)->value.Next() - found->value.Next();
      return true;
    }

    static std::size_t Size(std::size_t entries) {
      return (1 + entries) * sizeof(Entry);
    }

    std::size_t InsertOffset() const {
      return end_ - begin_;
    }

    class Inserter {
      public:
        Inserter(SimpleTrie<Value> &in, std::size_t size) : end_(in.end_) {}

        void Add(WordIndex key, const Value &value) {
          end_->key = key;
          end_->value = value;
          ++end_;
        }

      private:
        Entry *&end_;
    };

  private:
    friend class Inserter;
    const Entry *begin_;
    Entry *end_;

    std::size_t total_entries_;
};*/

} // namespace trie
} // namespace lm

#endif // LM_TRIE_NODE__

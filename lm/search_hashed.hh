#ifndef LM_SEARCH_HASHED__
#define LM_SEARCH_HASHED__

#include "lm/binary_format.hh"
#include "lm/config.hh"
#include "lm/read_arpa.hh"
#include "lm/weights.hh"

#include "util/key_value_packing.hh"
#include "util/probing_hash_table.hh"

#include <algorithm>
#include <vector>

namespace util { class FilePiece; }

namespace lm {
namespace ngram {
struct Backing;
namespace detail {

inline uint64_t CombineWordHash(uint64_t current, const WordIndex next) {
  uint64_t ret = (current * 8978948897894561157ULL) ^ (static_cast<uint64_t>(1 + next) * 17894857484156487943ULL);
  return ret;
}

struct HashedSearch {
  typedef uint64_t Node;

  class Unigram {
    public:
      Unigram() {}

      Unigram(void *start, std::size_t /*allocated*/) : unigram_(static_cast<ProbBackoff*>(start)) {}

      static std::size_t Size(uint64_t count) {
        return (count + 1) * sizeof(ProbBackoff); // +1 for hallucinate <unk>
      }

      const ProbBackoff &Lookup(WordIndex index) const { return unigram_[index]; }

      ProbBackoff &Unknown() { return unigram_[0]; }

      void LoadedBinary() {}

      // For building.
      ProbBackoff *Raw() { return unigram_; }

    private:
      ProbBackoff *unigram_;
  };

  Unigram unigram;

  void LookupUnigram(WordIndex word, float &prob, float &backoff, Node &next) const {
    const ProbBackoff &entry = unigram.Lookup(word);
    prob = entry.prob;
    backoff = entry.backoff;
    next = static_cast<Node>(word);
  }
};

template <class MiddleT, class LongestT> class TemplateHashedSearch : public HashedSearch {
  public:
    typedef MiddleT Middle;

    typedef LongestT Longest;
    Longest longest;

    // TODO: move probing_multiplier here with next binary file format update.  
    static void UpdateConfigFromBinary(int, const std::vector<uint64_t> &, Config &) {}

    static std::size_t Size(const std::vector<uint64_t> &counts, const Config &config) {
      std::size_t ret = Unigram::Size(counts[0]);
      for (unsigned char n = 1; n < counts.size() - 1; ++n) {
        ret += Middle::Size(counts[n], config.probing_multiplier);
      }
      return ret + Longest::Size(counts.back(), config.probing_multiplier);
    }

    uint8_t *SetupMemory(uint8_t *start, const std::vector<uint64_t> &counts, const Config &config);

    template <class Voc> void InitializeFromARPA(const char *file, util::FilePiece &f, const std::vector<uint64_t> &counts, const Config &config, Voc &vocab, Backing &backing);

    const Middle *MiddleBegin() const { return &*middle_.begin(); }
    const Middle *MiddleEnd() const { return &*middle_.end(); }

    bool LookupMiddle(const Middle &middle, WordIndex word, float &prob, float &backoff, Node &node) const {
      node = CombineWordHash(node, word);
      typename Middle::ConstIterator found;
      if (!middle.Find(node, found)) return false;
      prob = found->GetValue().prob;
      backoff = found->GetValue().backoff;
      return true;
    }

    void LoadedBinary();

    bool LookupMiddleNoProb(const Middle &middle, WordIndex word, float &backoff, Node &node) const {
      node = CombineWordHash(node, word);
      typename Middle::ConstIterator found;
      if (!middle.Find(node, found)) return false;
      backoff = found->GetValue().backoff;
      return true;
    }

    bool LookupLongest(WordIndex word, float &prob, Node &node) const {
      node = CombineWordHash(node, word);
      typename Longest::ConstIterator found;
      if (!longest.Find(node, found)) return false;
      prob = found->GetValue().prob;
      return true;
    }

    // Geenrate a node without necessarily checking that it actually exists.  
    // Optionally return false if it's know to not exist.  
    bool FastMakeNode(const WordIndex *begin, const WordIndex *end, Node &node) const {
      assert(begin != end);
      node = static_cast<Node>(*begin);
      for (const WordIndex *i = begin + 1; i < end; ++i) {
        node = CombineWordHash(node, *i);
      }
      return true;
    }

  private:
    std::vector<Middle> middle_;
};

// std::identity is an SGI extension :-(
struct IdentityHash : public std::unary_function<uint64_t, size_t> {
  size_t operator()(uint64_t arg) const { return static_cast<size_t>(arg); }
};

struct ProbingHashedSearch : public TemplateHashedSearch<
  util::ProbingHashTable<util::ByteAlignedPacking<uint64_t, ProbBackoff>, IdentityHash>,
  util::ProbingHashTable<util::ByteAlignedPacking<uint64_t, Prob>, IdentityHash> > {

  static const ModelType kModelType = HASH_PROBING;
};

} // namespace detail
} // namespace ngram
} // namespace lm

#endif // LM_SEARCH_HASHED__

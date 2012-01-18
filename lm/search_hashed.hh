#ifndef LM_SEARCH_HASHED__
#define LM_SEARCH_HASHED__

#include "lm/model_type.hh"
#include "lm/config.hh"
#include "lm/read_arpa.hh"
#include "lm/return.hh"
#include "lm/weights.hh"

#include "util/bit_packing.hh"
#include "util/probing_hash_table.hh"

#include <algorithm>
#include <iostream>
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

  void LookupUnigram(WordIndex word, float &backoff, Node &next, FullScoreReturn &ret) const {
    const ProbBackoff &entry = unigram.Lookup(word);
    util::FloatEnc val;
    val.f = entry.prob;
    ret.independent_left = (val.i & util::kSignBit);
    ret.extend_left = static_cast<uint64_t>(word);
    val.i |= util::kSignBit;
    ret.prob = val.f;
    backoff = entry.backoff;
    next = static_cast<Node>(word);
  }
};

template <class MiddleT, class LongestT> class TemplateHashedSearch : public HashedSearch {
  public:
    typedef MiddleT Middle;

    typedef LongestT Longest;
    Longest longest;

    static const unsigned int kVersion = 0;

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

    typedef typename std::vector<Middle>::const_iterator MiddleIter;

    MiddleIter MiddleBegin() const { return middle_.begin(); }
    MiddleIter MiddleEnd() const { return middle_.end(); }

    Node Unpack(uint64_t extend_pointer, unsigned char extend_length, float &prob) const {
      util::FloatEnc val;
      if (extend_length == 1) {
        val.f = unigram.Lookup(static_cast<uint64_t>(extend_pointer)).prob;
      } else {
        typename Middle::ConstIterator found;
        if (!middle_[extend_length - 2].Find(extend_pointer, found)) {
          std::cerr << "Extend pointer " << extend_pointer << " should have been found for length " << (unsigned) extend_length << std::endl;
          abort();
        }
        val.f = found->value.prob;
      }
      val.i |= util::kSignBit;
      prob = val.f;
      return extend_pointer;
    }

    bool LookupMiddle(const Middle &middle, WordIndex word, float &backoff, Node &node, FullScoreReturn &ret) const {
      node = CombineWordHash(node, word);
      typename Middle::ConstIterator found;
      if (!middle.Find(node, found)) return false;
      util::FloatEnc enc;
      enc.f = found->value.prob;
      ret.independent_left = (enc.i & util::kSignBit);
      ret.extend_left = node;
      enc.i |= util::kSignBit;
      ret.prob = enc.f;
      backoff = found->value.backoff;
      return true;
    }

    void LoadedBinary();

    bool LookupMiddleNoProb(const Middle &middle, WordIndex word, float &backoff, Node &node) const {
      node = CombineWordHash(node, word);
      typename Middle::ConstIterator found;
      if (!middle.Find(node, found)) return false;
      backoff = found->value.backoff;
      return true;
    }

    bool LookupLongest(WordIndex word, float &prob, Node &node) const {
      // Sign bit is always on because longest n-grams do not extend left.  
      node = CombineWordHash(node, word);
      typename Longest::ConstIterator found;
      if (!longest.Find(node, found)) return false;
      prob = found->value.prob;
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

/* These look like perfect candidates for a template, right?  Ancient gcc (4.1
 * on RedHat stale linux) doesn't pack templates correctly.  ProbBackoffEntry
 * is a multiple of 8 bytes anyway.  ProbEntry is 12 bytes so it's set to pack.
 */
struct ProbBackoffEntry {
  uint64_t key;
  ProbBackoff value;
  typedef uint64_t Key;
  typedef ProbBackoff Value;
  uint64_t GetKey() const {
    return key;
  }
  static ProbBackoffEntry Make(uint64_t key, ProbBackoff value) {
    ProbBackoffEntry ret;
    ret.key = key;
    ret.value = value;
    return ret;
  }
};

#pragma pack(push)
#pragma pack(4)
struct ProbEntry {
  uint64_t key;
  Prob value;
  typedef uint64_t Key;
  typedef Prob Value;
  uint64_t GetKey() const {
    return key;
  }
  static ProbEntry Make(uint64_t key, Prob value) {
    ProbEntry ret;
    ret.key = key;
    ret.value = value;
    return ret;
  }
};

#pragma pack(pop)


struct ProbingHashedSearch : public TemplateHashedSearch<
  util::ProbingHashTable<ProbBackoffEntry, util::IdentityHash>,
  util::ProbingHashTable<ProbEntry, util::IdentityHash> > {

  static const ModelType kModelType = HASH_PROBING;
};

} // namespace detail
} // namespace ngram
} // namespace lm

#endif // LM_SEARCH_HASHED__

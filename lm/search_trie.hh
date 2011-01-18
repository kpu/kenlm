#ifndef LM_SEARCH_TRIE__
#define LM_SEARCH_TRIE__

#include "lm/binary_format.hh"
#include "lm/trie.hh"
#include "lm/weights.hh"

#include <assert.h>

namespace lm {
namespace ngram {
struct Backing;
class SortedVocabulary;
namespace trie {

struct TrieSearch {
  typedef NodeRange Node;

  typedef ::lm::ngram::trie::Unigram Unigram;
  Unigram unigram;

  typedef trie::BitPackedMiddle Middle;
  std::vector<Middle> middle;

  typedef trie::BitPackedLongest Longest;
  Longest longest;

  static const ModelType kModelType = TRIE_SORTED;

  static std::size_t Size(const std::vector<uint64_t> &counts, const Config &/*config*/) {
    std::size_t ret = Unigram::Size(counts[0]);
    for (unsigned char i = 1; i < counts.size() - 1; ++i) {
      ret += Middle::Size(counts[i], counts[0], counts[i+1]);
    }
    return ret + Longest::Size(counts.back(), counts[0]);
  }

  uint8_t *SetupMemory(uint8_t *start, const std::vector<uint64_t> &counts, const Config &/*config*/) {
    unigram.Init(start);
    start += Unigram::Size(counts[0]);
    middle.resize(counts.size() - 2);
    for (unsigned char i = 1; i < counts.size() - 1; ++i) {
      middle[i-1].Init(
          start,
          counts[0],
          counts[i+1], 
          (i == counts.size() - 2) ? static_cast<const BitPacked&>(longest) : static_cast<const BitPacked &>(middle[i]));
      start += Middle::Size(counts[i], counts[0], counts[i+1]);
    }
    longest.Init(start, counts[0]);
    return start + Longest::Size(counts.back(), counts[0]);
  }

  void InitializeFromARPA(const char *file, util::FilePiece &f, std::vector<uint64_t> &counts, const Config &config, SortedVocabulary &vocab, Backing &backing);

  bool LookupUnigram(WordIndex word, float &prob, float &backoff, Node &node) const {
    return unigram.Find(word, prob, backoff, node);
  }

  bool LookupMiddle(const Middle &mid, WordIndex word, float &prob, float &backoff, Node &node) const {
    return mid.Find(word, prob, backoff, node);
  }

  bool LookupMiddleNoProb(const Middle &mid, WordIndex word, float &backoff, Node &node) const {
    return mid.FindNoProb(word, backoff, node);
  }

  bool LookupLongest(WordIndex word, float &prob, const Node &node) const {
    return longest.Find(word, prob, node);
  }

  bool FastMakeNode(const WordIndex *begin, const WordIndex *end, Node &node) const {
    // TODO: don't decode backoff.
    assert(begin != end);
    float ignored_prob, ignored_backoff;
    LookupUnigram(*begin, ignored_prob, ignored_backoff, node);
    for (const WordIndex *i = begin + 1; i < end; ++i) {
      if (!LookupMiddleNoProb(middle[i - begin - 1], *i, ignored_backoff, node)) return false;
    }
    return true;
  }
};

} // namespace trie
} // namespace ngram
} // namespace lm

#endif // LM_SEARCH_TRIE__

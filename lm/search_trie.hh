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

template <class Quant> class TrieSearch;
template <class Quant> void BuildTrie(const std::string &file_prefix, std::vector<uint64_t> &counts, const Config &config, TrieSearch<Quant> &out, Backing     &backing);

template <class Quant> class TrieSearch {
  public:
    typedef NodeRange Node;

    typedef ::lm::ngram::trie::Unigram Unigram;
    Unigram unigram;

    typedef trie::BitPackedMiddle<typename Quant::Middle> Middle;

    typedef trie::BitPackedLongest<typename Quant::Longest> Longest;
    Longest longest;

    static const ModelType kModelType = TRIE_SORTED;

    static std::size_t Size(const std::vector<uint64_t> &counts, const Config &config) {
      std::size_t ret = Quant::Size(counts, config) + Unigram::Size(counts[0]);
      for (unsigned char i = 1; i < counts.size() - 1; ++i) {
        ret += Middle::Size(Quant::MiddleBits(config), counts[i], counts[0], counts[i+1]);
      }
      return ret + Longest::Size(Quant::LongestBits(config), counts.back(), counts[0]);
    }

    TrieSearch() : middle_begin_(NULL), middle_end_(NULL) {}

    ~TrieSearch() { FreeMiddles(); }

    uint8_t *SetupMemory(uint8_t *start, const std::vector<uint64_t> &counts, const Config &config) {
      quant_.Init(start, counts, config);
      start += Quant::Size(counts, config);
      unigram.Init(start);
      start += Unigram::Size(counts[0]);
      FreeMiddles();
      middle_begin_ = static_cast<Middle*>(malloc(sizeof(Middle) * (counts.size() - 2)));
      middle_end_ = middle_begin_ + (counts.size() - 2);
      for (unsigned char i = counts.size() - 1; i >= 2; --i) {
        new (middle_begin_ + i - 2) Middle(
            start,
            quant_.Mid(i),
            counts[0],
            counts[i], 
            (i == counts.size() - 1) ? static_cast<const BitPacked&>(longest) : static_cast<const BitPacked &>(middle_begin_[i-1]));
        start += Middle::Size(63, counts[i-1], counts[0], counts[i]);
      }
      longest.Init(start, quant_.Long(counts.size()), counts[0]);
      return start + Longest::Size(31, counts.back(), counts[0]);
    }

    void LoadedBinary();

    const Middle *MiddleBegin() const { return middle_begin_; }
    const Middle *MiddleEnd() const { return middle_end_; }

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
        if (!LookupMiddleNoProb(middle_begin_[i - begin - 1], *i, ignored_backoff, node)) return false;
      }
      return true;
    }

  private:
    friend void BuildTrie<Quant>(const std::string &file_prefix, std::vector<uint64_t> &counts, const Config &config, TrieSearch<Quant> &out, Backing     &backing);

    void FreeMiddles() {
      for (const Middle *i = middle_begin_; i != middle_end_; ++i) {
        i->~Middle();
      }
      free(middle_begin_);
    }

    Middle *middle_begin_, *middle_end_;
    Quant quant_;
};

} // namespace trie
} // namespace ngram
} // namespace lm

#endif // LM_SEARCH_TRIE__

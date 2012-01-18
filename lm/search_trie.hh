#ifndef LM_SEARCH_TRIE__
#define LM_SEARCH_TRIE__

#include "lm/config.hh"
#include "lm/model_type.hh"
#include "lm/return.hh"
#include "lm/trie.hh"
#include "lm/weights.hh"

#include "util/file.hh"
#include "util/file_piece.hh"

#include <vector>

#include <assert.h>

namespace lm {
namespace ngram {
struct Backing;
class SortedVocabulary;
namespace trie {

template <class Quant, class Bhiksha> class TrieSearch;
class SortedFiles;
template <class Quant, class Bhiksha> void BuildTrie(SortedFiles &files, std::vector<uint64_t> &counts, const Config &config, TrieSearch<Quant, Bhiksha> &out, Quant &quant, const SortedVocabulary &vocab, Backing &backing);

template <class Quant, class Bhiksha> class TrieSearch {
  public:
    typedef NodeRange Node;

    typedef ::lm::ngram::trie::Unigram Unigram;
    Unigram unigram;

    typedef trie::BitPackedMiddle<typename Quant::Middle, Bhiksha> Middle;

    typedef trie::BitPackedLongest<typename Quant::Longest> Longest;
    Longest longest;

    static const ModelType kModelType = static_cast<ModelType>(TRIE_SORTED + Quant::kModelTypeAdd + Bhiksha::kModelTypeAdd);

    static const unsigned int kVersion = 1;

    static void UpdateConfigFromBinary(int fd, const std::vector<uint64_t> &counts, Config &config) {
      Quant::UpdateConfigFromBinary(fd, counts, config);
      util::AdvanceOrThrow(fd, Quant::Size(counts.size(), config) + Unigram::Size(counts[0]));
      Bhiksha::UpdateConfigFromBinary(fd, config);
    }

    static std::size_t Size(const std::vector<uint64_t> &counts, const Config &config) {
      std::size_t ret = Quant::Size(counts.size(), config) + Unigram::Size(counts[0]);
      for (unsigned char i = 1; i < counts.size() - 1; ++i) {
        ret += Middle::Size(Quant::MiddleBits(config), counts[i], counts[0], counts[i+1], config);
      }
      return ret + Longest::Size(Quant::LongestBits(config), counts.back(), counts[0]);
    }

    TrieSearch() : middle_begin_(NULL), middle_end_(NULL) {}

    ~TrieSearch() { FreeMiddles(); }

    uint8_t *SetupMemory(uint8_t *start, const std::vector<uint64_t> &counts, const Config &config);

    void LoadedBinary();

    typedef const Middle *MiddleIter;

    const Middle *MiddleBegin() const { return middle_begin_; }
    const Middle *MiddleEnd() const { return middle_end_; }

    void InitializeFromARPA(const char *file, util::FilePiece &f, std::vector<uint64_t> &counts, const Config &config, SortedVocabulary &vocab, Backing &backing);

    void LookupUnigram(WordIndex word, float &backoff, Node &node, FullScoreReturn &ret) const {
      unigram.Find(word, ret.prob, backoff, node);
      ret.independent_left = (node.begin == node.end);
      ret.extend_left = static_cast<uint64_t>(word);
    }

    bool LookupMiddle(const Middle &mid, WordIndex word, float &backoff, Node &node, FullScoreReturn &ret) const {
      if (!mid.Find(word, ret.prob, backoff, node, ret.extend_left)) return false;
      ret.independent_left = (node.begin == node.end);
      return true;
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
      FullScoreReturn ignored;
      float ignored_backoff;
      LookupUnigram(*begin, ignored_backoff, node, ignored);
      for (const WordIndex *i = begin + 1; i < end; ++i) {
        if (!LookupMiddleNoProb(middle_begin_[i - begin - 1], *i, ignored_backoff, node)) return false;
      }
      return true;
    }

    Node Unpack(uint64_t extend_pointer, unsigned char extend_length, float &prob) const {
      if (extend_length == 1) {
        float ignored;
        Node ret;
        unigram.Find(static_cast<WordIndex>(extend_pointer), prob, ignored, ret);
        return ret;
      }
      return middle_begin_[extend_length - 2].ReadEntry(extend_pointer, prob);
    }

  private:
    friend void BuildTrie<Quant, Bhiksha>(SortedFiles &files, std::vector<uint64_t> &counts, const Config &config, TrieSearch<Quant, Bhiksha> &out, Quant &quant, const SortedVocabulary &vocab, Backing &backing);

    // Middles are managed manually so we can delay construction and they don't have to be copyable.  
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

#ifndef LM_NGRAM__
#define LM_NGRAM__

#include "lm/binary_format.hh"
#include "lm/facade.hh"
#include "lm/ngram_config.hh"
#include "lm/ngram_hashed.hh"
#include "lm/ngram_trie.hh"
#include "lm/vocab.hh"
#include "lm/weights.hh"

#include <algorithm>
#include <vector>

namespace util { class FilePiece; }

namespace lm {
namespace ngram {

// If you need higher order, change this and recompile.  
// Having this limit means that State can be
// (kMaxOrder - 1) * sizeof(float) bytes instead of
// sizeof(float*) + (kMaxOrder - 1) * sizeof(float) + malloc overhead
const std::size_t kMaxOrder = 6;

// This is a POD.  
class State {
  public:
    bool operator==(const State &other) const {
      if (valid_length_ != other.valid_length_) return false;
      const WordIndex *end = history_ + valid_length_;
      for (const WordIndex *first = history_, *second = other.history_;
          first != end; ++first, ++second) {
        if (*first != *second) return false;
      }
      // If the histories are equal, so are the backoffs.  
      return true;
    }

    // You shouldn't need to touch anything below this line, but the members are public so FullState will qualify as a POD.  
    // This order minimizes total size of the struct if WordIndex is 64 bit, float is 32 bit, and alignment of 64 bit integers is 64 bit.  
    WordIndex history_[kMaxOrder - 1];
    float backoff_[kMaxOrder - 1];
    unsigned char valid_length_;
};

size_t hash_value(const State &state);

namespace detail {

// Should return the same results as SRI.  
// Why VocabularyT instead of just Vocabulary?  ModelFacade defines Vocabulary.  
template <class Search, class VocabularyT> class GenericModel : public base::ModelFacade<GenericModel<Search, VocabularyT>, State, VocabularyT> {
  private:
    typedef base::ModelFacade<GenericModel<Search, VocabularyT>, State, VocabularyT> P;
  public:
    // Get the size of memory that will be mapped given ngram counts.  This
    // does not include small non-mapped control structures, such as this class
    // itself.  
    static size_t Size(const std::vector<uint64_t> &counts, const Config &config = Config());

    GenericModel(const char *file, const Config &config = Config());

    FullScoreReturn FullScore(const State &in_state, const WordIndex new_word, State &out_state) const;

    /* Slower call without in_state.  Don't use this if you can avoid it.  This
     * is mostly a hack for Hieu to integrate it into Moses which sometimes
     * forgets LM state (i.e. it doesn't store it with the phrase).  Sigh.   
     * The context indices should be in an array.  
     * If context_rbegin != context_rend then *context_rbegin is the word
     * before new_word.  
     */
    FullScoreReturn FullScoreForgotState(const WordIndex *context_rbegin, const WordIndex *context_rend, const WordIndex new_word, State &out_state) const;

    /* Get the state for a context.  Don't use this if you can avoid it.  Use
     * BeginSentenceState or EmptyContextState and extend from those.  If
     * you're only going to use this state to call FullScore once, use
     * FullScoreForgotState. */
    void GetState(const WordIndex *context_rbegin, const WordIndex *context_rend, State &out_state) const;

  private:
    friend void LoadLM<>(const char *file, const Config &config, GenericModel<Search, VocabularyT> &to);

    float SlowBackoffLookup(const WordIndex *const context_rbegin, const WordIndex *const context_rend, unsigned char start) const;

    FullScoreReturn ScoreExceptBackoff(const WordIndex *context_rbegin, const WordIndex *context_rend, const WordIndex new_word, unsigned char &backoff_start, State &out_state) const;

    // Appears after Size in the cc file.
    void SetupMemory(void *start, const std::vector<uint64_t> &counts, const Config &config);

    void InitializeFromBinary(void *start, const Parameters &params, const Config &config, int fd);

    void InitializeFromARPA(const char *file, util::FilePiece &f, void *start, const Parameters &params, const Config &config);

    Backing &MutableBacking() { return backing_; }

    static const ModelType kModelType = Search::kModelType;

    Backing backing_;
    
    VocabularyT vocab_;

    typedef typename Search::Unigram Unigram;
    typedef typename Search::Middle Middle;
    typedef typename Search::Longest Longest;

    Search search_;
};

} // namespace detail

// These must also be instantiated in the cc file.  
typedef ::lm::ngram::ProbingVocabulary Vocabulary;
typedef detail::GenericModel<detail::ProbingHashedSearch, Vocabulary> ProbingModel;
// Default implementation.  No real reason for it to be the default.  
typedef ProbingModel Model;

typedef ::lm::ngram::SortedVocabulary SortedVocabulary;
typedef detail::GenericModel<detail::SortedHashedSearch, SortedVocabulary> SortedModel;

typedef detail::GenericModel<trie::TrieSearch, SortedVocabulary> TrieModel;

} // namespace ngram
} // namespace lm

#endif // LM_NGRAM__

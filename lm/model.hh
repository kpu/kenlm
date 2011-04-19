#ifndef LM_MODEL__
#define LM_MODEL__

#include "lm/binary_format.hh"
#include "lm/config.hh"
#include "lm/facade.hh"
#include "lm/max_order.hh"
#include "lm/search_hashed.hh"
#include "lm/search_trie.hh"
#include "lm/vocab.hh"
#include "lm/weights.hh"

#include <algorithm>
#include <vector>

#include <string.h>

namespace util { class FilePiece; }

namespace lm {
namespace ngram {

// This is a POD but if you want memcmp to return the same as operator==, call
// ZeroRemaining first.    
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

    // Three way comparison function.  
    int Compare(const State &other) const {
      if (valid_length_ == other.valid_length_) {
        return memcmp(history_, other.history_, valid_length_ * sizeof(WordIndex));
      }
      return (valid_length_ < other.valid_length_) ? -1 : 1;
    }

    // Call this before using raw memcmp.  
    void ZeroRemaining() {
      for (unsigned char i = valid_length_; i < kMaxOrder - 1; ++i) {
        history_[i] = 0;
        backoff_[i] = 0.0;
      }
    }

    unsigned char ValidLength() const { return valid_length_; }

    // You shouldn't need to touch anything below this line, but the members are public so FullState will qualify as a POD.  
    // This order minimizes total size of the struct if WordIndex is 64 bit, float is 32 bit, and alignment of 64 bit integers is 64 bit.  
    WordIndex history_[kMaxOrder - 1];
    float backoff_[kMaxOrder - 1];
    unsigned char valid_length_;
};

size_t hash_value(const State &state);

namespace detail {

// Should return the same results as SRI.  
// ModelFacade typedefs Vocabulary so we use VocabularyT to avoid naming conflicts.
template <class Search, class VocabularyT> class GenericModel : public base::ModelFacade<GenericModel<Search, VocabularyT>, State, VocabularyT> {
  private:
    typedef base::ModelFacade<GenericModel<Search, VocabularyT>, State, VocabularyT> P;
  public:
    // Get the size of memory that will be mapped given ngram counts.  This
    // does not include small non-mapped control structures, such as this class
    // itself.  
    static size_t Size(const std::vector<uint64_t> &counts, const Config &config = Config());

    /* Load the model from a file.  It may be an ARPA or binary file.  Binary
     * files must have the format expected by this class or you'll get an
     * exception.  So TrieModel can only load ARPA or binary created by
     * TrieModel.  To classify binary files, call RecognizeBinary in
     * lm/binary_format.hh.  
     */
    GenericModel(const char *file, const Config &config = Config());

    /* Score p(new_word | in_state) and incorporate new_word into out_state.
     * Note that in_state and out_state must be different references:
     * &in_state != &out_state.  
     */
    FullScoreReturn FullScore(const State &in_state, const WordIndex new_word, State &out_state) const;

    /* Slower call without in_state.  Try to remember state, but sometimes it
     * would cost too much memory or your decoder isn't setup properly.  
     * To use this function, make an array of WordIndex containing the context
     * vocabulary ids in reverse order.  Then, pass the bounds of the array:
     * [context_rbegin, context_rend).  The new_word is not part of the context
     * array unless you intend to repeat words.  
     */
    FullScoreReturn FullScoreForgotState(const WordIndex *context_rbegin, const WordIndex *context_rend, const WordIndex new_word, State &out_state) const;

    /* Get the state for a context.  Don't use this if you can avoid it.  Use
     * BeginSentenceState or EmptyContextState and extend from those.  If
     * you're only going to use this state to call FullScore once, use
     * FullScoreForgotState. 
     * To use this function, make an array of WordIndex containing the context
     * vocabulary ids in reverse order.  Then, pass the bounds of the array:
     * [context_rbegin, context_rend).  
     */
    void GetState(const WordIndex *context_rbegin, const WordIndex *context_rend, State &out_state) const;

  private:
    friend void LoadLM<>(const char *file, const Config &config, GenericModel<Search, VocabularyT> &to);

    float SlowBackoffLookup(const WordIndex *const context_rbegin, const WordIndex *const context_rend, unsigned char start) const;

    FullScoreReturn ScoreExceptBackoff(const WordIndex *context_rbegin, const WordIndex *context_rend, const WordIndex new_word, State &out_state) const;

    // Appears after Size in the cc file.
    void SetupMemory(void *start, const std::vector<uint64_t> &counts, const Config &config);

    void InitializeFromBinary(void *start, const Parameters &params, const Config &config, int fd);

    void InitializeFromARPA(const char *file, const Config &config);

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

// Smaller implementation.
typedef ::lm::ngram::SortedVocabulary SortedVocabulary;
typedef detail::GenericModel<trie::TrieSearch, SortedVocabulary> TrieModel;

} // namespace ngram
} // namespace lm

#endif // LM_MODEL__

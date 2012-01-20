#ifndef LM_MODEL__
#define LM_MODEL__

#include "lm/bhiksha.hh"
#include "lm/binary_format.hh"
#include "lm/config.hh"
#include "lm/facade.hh"
#include "lm/max_order.hh"
#include "lm/quantize.hh"
#include "lm/search_hashed.hh"
#include "lm/search_trie.hh"
#include "lm/vocab.hh"
#include "lm/weights.hh"

#include "util/murmur_hash.hh"

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
      if (length != other.length) return false;
      return !memcmp(words, other.words, length * sizeof(WordIndex));
    }

    // Three way comparison function.  
    int Compare(const State &other) const {
      if (length != other.length) return length < other.length ? -1 : 1;
      return memcmp(words, other.words, length * sizeof(WordIndex));
    }

    bool operator<(const State &other) const {
      if (length != other.length) return length < other.length;
      return memcmp(words, other.words, length * sizeof(WordIndex)) < 0;
    }

    // Call this before using raw memcmp.  
    void ZeroRemaining() {
      for (unsigned char i = length; i < kMaxOrder - 1; ++i) {
        words[i] = 0;
        backoff[i] = 0.0;
      }
    }

    unsigned char Length() const { return length; }

    // You shouldn't need to touch anything below this line, but the members are public so FullState will qualify as a POD.  
    // This order minimizes total size of the struct if WordIndex is 64 bit, float is 32 bit, and alignment of 64 bit integers is 64 bit.  
    WordIndex words[kMaxOrder - 1];
    float backoff[kMaxOrder - 1];
    unsigned char length;
};

inline size_t hash_value(const State &state) {
  return util::MurmurHashNative(state.words, sizeof(WordIndex) * state.length);
}

namespace detail {

// Should return the same results as SRI.  
// ModelFacade typedefs Vocabulary so we use VocabularyT to avoid naming conflicts.
template <class Search, class VocabularyT> class GenericModel : public base::ModelFacade<GenericModel<Search, VocabularyT>, State, VocabularyT> {
  private:
    typedef base::ModelFacade<GenericModel<Search, VocabularyT>, State, VocabularyT> P;
  public:
    // This is the model type returned by RecognizeBinary.
    static const ModelType kModelType;

    static const unsigned int kVersion = Search::kVersion;

    /* Get the size of memory that will be mapped given ngram counts.  This
     * does not include small non-mapped control structures, such as this class
     * itself.  
     */
    static size_t Size(const std::vector<uint64_t> &counts, const Config &config = Config());

    /* Load the model from a file.  It may be an ARPA or binary file.  Binary
     * files must have the format expected by this class or you'll get an
     * exception.  So TrieModel can only load ARPA or binary created by
     * TrieModel.  To classify binary files, call RecognizeBinary in
     * lm/binary_format.hh.  
     */
    explicit GenericModel(const char *file, const Config &config = Config());

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

    /* More efficient version of FullScore where a partial n-gram has already
     * been scored.  
     * NOTE: THE RETURNED .prob IS RELATIVE, NOT ABSOLUTE.  So for example, if
     * the n-gram does not end up extending further left, then 0 is returned.
     */
    FullScoreReturn ExtendLeft(
        // Additional context in reverse order.  This will update add_rend to 
        const WordIndex *add_rbegin, const WordIndex *add_rend,
        // Backoff weights to use.  
        const float *backoff_in,
        // extend_left returned by a previous query.
        uint64_t extend_pointer,
        // Length of n-gram that the pointer corresponds to.  
        unsigned char extend_length,
        // Where to write additional backoffs for [extend_length + 1, min(Order() - 1, return.ngram_length)]
        float *backoff_out,
        // Amount of additional content that should be considered by the next call.
        unsigned char &next_use) const;

  private:
    friend void lm::ngram::LoadLM<>(const char *file, const Config &config, GenericModel<Search, VocabularyT> &to);

    static void UpdateConfigFromBinary(int fd, const std::vector<uint64_t> &counts, Config &config);

    FullScoreReturn ScoreExceptBackoff(const WordIndex *context_rbegin, const WordIndex *context_rend, const WordIndex new_word, State &out_state) const;

    // Appears after Size in the cc file.
    void SetupMemory(void *start, const std::vector<uint64_t> &counts, const Config &config);

    void InitializeFromBinary(void *start, const Parameters &params, const Config &config, int fd);

    void InitializeFromARPA(const char *file, const Config &config);

    Backing &MutableBacking() { return backing_; }

    Backing backing_;
    
    VocabularyT vocab_;

    typedef typename Search::Middle Middle;

    Search search_;
};

} // namespace detail

// These must also be instantiated in the cc file.  
typedef ::lm::ngram::ProbingVocabulary Vocabulary;
typedef detail::GenericModel<detail::ProbingHashedSearch, Vocabulary> ProbingModel; // HASH_PROBING
// Default implementation.  No real reason for it to be the default.  
typedef ProbingModel Model;

// Smaller implementation.
typedef ::lm::ngram::SortedVocabulary SortedVocabulary;
typedef detail::GenericModel<trie::TrieSearch<DontQuantize, trie::DontBhiksha>, SortedVocabulary> TrieModel; // TRIE_SORTED
typedef detail::GenericModel<trie::TrieSearch<DontQuantize, trie::ArrayBhiksha>, SortedVocabulary> ArrayTrieModel;

typedef detail::GenericModel<trie::TrieSearch<SeparatelyQuantize, trie::DontBhiksha>, SortedVocabulary> QuantTrieModel; // QUANT_TRIE_SORTED
typedef detail::GenericModel<trie::TrieSearch<SeparatelyQuantize, trie::ArrayBhiksha>, SortedVocabulary> QuantArrayTrieModel;

} // namespace ngram
} // namespace lm

#endif // LM_MODEL__

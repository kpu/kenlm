#ifndef LM_NGRAM__
#define LM_NGRAM__

#include "lm/facade.hh"
#include "lm/ngram_config.hh"
#include "lm/vocab.hh"
#include "lm/weights.hh"
#include "util/key_value_packing.hh"
#include "util/mmap.hh"
#include "util/probing_hash_table.hh"
#include "util/scoped.hh"
#include "util/sorted_uniform.hh"
#include "util/string_piece.hh"

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

// std::identity is an SGI extension :-(
struct IdentityHash : public std::unary_function<uint64_t, size_t> {
  size_t operator()(uint64_t arg) const { return static_cast<size_t>(arg); }
};

// Should return the same results as SRI.  
// Why VocabularyT instead of just Vocabulary?  ModelFacade defines Vocabulary.  
template <class Search, class VocabularyT> class GenericModel : public base::ModelFacade<GenericModel<Search, VocabularyT>, State, VocabularyT> {
  private:
    typedef base::ModelFacade<GenericModel<Search, VocabularyT>, State, VocabularyT> P;
  public:
    // Get the size of memory that will be mapped given ngram counts.  This
    // does not include small non-mapped control structures, such as this class
    // itself.  
    static size_t Size(const std::vector<size_t> &counts, const Config &config = Config());

    GenericModel(const char *file, Config config = Config());

    FullScoreReturn FullScore(const State &in_state, const WordIndex new_word, State &out_state) const;

  private:
    // Appears after Size in the cc.  
    void SetupMemory(char *start, const std::vector<size_t> &counts, const Config &config);

    void LoadFromARPA(util::FilePiece &f, const std::vector<size_t> &counts, const Config &config);

    util::scoped_fd mapped_file_;

    // memory_ is the raw block of memory backing vocab_, unigram_, [middle.begin(), middle.end()), and longest_.  
    util::scoped_mmap memory_;
    
    VocabularyT vocab_;

    ProbBackoff *unigram_;

    typedef typename Search::template Table<ProbBackoff>::T Middle;
    std::vector<Middle> middle_;

    typedef typename Search::template Table<Prob>::T Longest;
    Longest longest_;
};

struct ProbingSearch {
  typedef float Init;

  static const unsigned char kBinaryTag = 1;

  template <class Value> struct Table {
    typedef util::ByteAlignedPacking<uint64_t, Value> Packing;
    typedef util::ProbingHashTable<Packing, IdentityHash> T;
  };
};

struct SortedUniformSearch {
  // This is ignored.
  typedef float Init;

  static const unsigned char kBinaryTag = 2;

  template <class Value> struct Table {
    typedef util::ByteAlignedPacking<uint64_t, Value> Packing;
    typedef util::SortedUniformMap<Packing> T;
  };
};

} // namespace detail

// These must also be instantiated in the cc file.  
typedef ::lm::ProbingVocabulary Vocabulary;
typedef detail::GenericModel<detail::ProbingSearch, Vocabulary> Model;

typedef ::lm::SortedVocabulary SortedVocabulary;
typedef detail::GenericModel<detail::SortedUniformSearch, SortedVocabulary> SortedModel;

} // namespace ngram
} // namespace lm

#endif // LM_NGRAM__

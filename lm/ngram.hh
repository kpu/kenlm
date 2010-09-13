#ifndef LM_NGRAM__
#define LM_NGRAM__

#include "lm/facade.hh"
#include "lm/ngram_config.hh"
#include "util/key_value_packing.hh"
#include "util/probing_hash_table.hh"
#include "util/sorted_uniform.hh"
#include "util/string_piece.hh"
#include "util/murmur_hash.hh"
#include "util/scoped.hh"

#include <algorithm>
#include <memory>
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

inline size_t hash_value(const State &state) {
  // If the histories are equal, so are the backoffs.  
  return util::MurmurHashNative(state.history_, sizeof(WordIndex) * state.valid_length_);
}

namespace detail {

inline uint64_t HashForVocab(const char *str, std::size_t len) {
  // This proved faster than Boost's hash in speed trials: total load time Murmur 67090000, Boost 72210000
  // Chose to use 64A instead of native so binary format will be portable across 64 and 32 bit.  
  return util::MurmurHash64A(str, len, 0);
}
inline uint64_t HashForVocab(const StringPiece &str) {
  return HashForVocab(str.data(), str.length());
}

struct Prob {
  float prob;
  void SetBackoff(float to);
  void ZeroBackoff() {}
};
// No inheritance so this will be a POD.  
struct ProbBackoff {
  float prob;
  float backoff;
  void SetBackoff(float to) { backoff = to; }
  void ZeroBackoff() { backoff = 0.0; }
};

} // namespace detail

// Vocabulary based on sorted uniform find storing only uint64_t values and using their offsets as indices.  
class SortedVocabulary : public base::Vocabulary {
  private:
    // Sorted uniform requires a GetKey function.  
    struct Entry {
      uint64_t GetKey() const { return key; }
      uint64_t key;
      bool operator<(const Entry &other) const {
        return key < other.key;
      }
    };

  public:
    SortedVocabulary();

    WordIndex Index(const StringPiece &str) const {
      const Entry *found;
      if (util::SortedUniformFind<const Entry *, uint64_t>(begin_, end_, detail::HashForVocab(str), found)) {
        return found - begin_ + 1; // +1 because <unk> is 0 and does not appear in the lookup table.
      } else {
        return 0;
      }
    }

    // Ignores second argument for consistency with probing hash which has a float here.  
    static size_t Size(std::size_t entries, float ignored = 0.0);

    // Everything else is for populating.  I'm too lazy to hide and friend these, but you'll only get a const reference anyway.
    void Init(void *start, std::size_t allocated, std::size_t entries);

    WordIndex Insert(const StringPiece &str);

    // Returns true if unknown was seen.  Reorders reorder_vocab so that the IDs are sorted.  
    bool FinishedLoading(detail::ProbBackoff *reorder_vocab);

    void LoadedBinary();

  private:
    Entry *begin_, *end_;

    bool saw_unk_;
};

namespace detail {

// Vocabulary storing a map from uint64_t to WordIndex.  
template <class Search> class MapVocabulary : public base::Vocabulary {
  public:
    MapVocabulary();

    WordIndex Index(const StringPiece &str) const {
      typename Lookup::ConstIterator i;
      return lookup_.Find(HashForVocab(str), i) ? i->GetValue() : 0;
    }

    static size_t Size(std::size_t entries, float probing_multiplier) {
      return Lookup::Size(entries, probing_multiplier);
    }

    // Everything else is for populating.  I'm too lazy to hide and friend these, but you'll only get a const reference anyway.
    void Init(void *start, std::size_t allocated, std::size_t entries);

    WordIndex Insert(const StringPiece &str);

    // Returns true if unknown was seen.  Does nothing with reorder_vocab.  
    bool FinishedLoading(ProbBackoff *reorder_vocab);

    void LoadedBinary();

  private:
    typedef typename Search::template Table<WordIndex>::T Lookup;
    Lookup lookup_;

    bool saw_unk_;
};

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
typedef detail::MapVocabulary<detail::ProbingSearch> Vocabulary;
typedef detail::GenericModel<detail::ProbingSearch, Vocabulary> Model;

// SortedVocabulary was defined above.  
typedef detail::GenericModel<detail::SortedUniformSearch, SortedVocabulary> SortedModel;

} // namespace ngram
} // namespace lm

#endif // LM_NGRAM__

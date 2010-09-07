#ifndef LM_NGRAM_H__
#define LM_NGRAM_H__

#include "lm/base.hh"
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
    unsigned char valid_length_;
    float backoff_[kMaxOrder - 1];
    WordIndex history_[kMaxOrder - 1];
};

inline size_t hash_value(const State &state) {
  // If the histories are equal, so are the backoffs.  
  return MurmurHash64A(state.history_, sizeof(WordIndex) * state.valid_length_, 0);
}

namespace detail {
// std::identity is an SGI extension :-(
struct IdentityHash : public std::unary_function<uint64_t, size_t> {
size_t operator()(uint64_t arg) const { return static_cast<size_t>(arg); }
};

template <class Search> class GenericVocabulary : public base::Vocabulary {
  public:
    GenericVocabulary() : next_(0) {}

    WordIndex Index(const StringPiece &str) const {
      const WordIndex *ret;
      return lookup_.Find(Hash(str), ret) ? *ret : not_found_;
    }

    static size_t Size(const typename Search::Init &search_init, std::size_t entries) {
      // +1 for possible unk token.
      return Lookup::Size(search_init, entries + 1);
    }

    // Everything else is for populating.  I'm too lazy to hide and friend these, but you'll only get a const reference anyway.
    void Init(const typename Search::Init &search_init, char *start, std::size_t entries);

    WordIndex Insert(const StringPiece &str);

    void FinishedLoading();

  private:
    static uint64_t Hash(const StringPiece &str) {
      // This proved faster than Boost's hash in speed trials: total load time Murmur 67090000, Boost 72210000
      return MurmurHash64A(reinterpret_cast<const void*>(str.data()), str.length(), 0);
    }
    bool Find(const StringPiece &str, WordIndex &found);

    typedef typename Search::template Table<WordIndex>::T Lookup;
    Lookup lookup_;

    WordIndex next_;
};

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

// Should return the same results as SRI except ln instead of log10
template <class Search> class GenericModel : public base::MiddleModel<GenericModel<Search>, State, GenericVocabulary<Search> > {
  private:
    typedef base::MiddleModel<GenericModel<Search>, State, GenericVocabulary<Search> > P;
  public:
    // Get the size of memory that will be mapped given ngram counts.  This
    // does not include small non-mapped control structures, such as this class
    // itself.  
    static size_t Size(const typename Search::Init &search_init, const std::vector<size_t> &counts);

    GenericModel(const char *file, const typename Search::Init &init);

    Return WithLength(const State &in_state, const WordIndex new_word, State &out_state) const;

  private:
    void LoadFromARPA(util::FilePiece &f, const std::vector<size_t> &counts);

    // memory_ is the raw block of memory backing vocab_, unigram_, [middle.begin(), middle.end()), and longest_.  
    util::scoped_mmap memory_;
    
    GenericVocabulary<Search> vocab_;
    WordIndex not_found_;

    ProbBackoff *unigram_;

    typedef typename Search::template Table<ProbBackoff>::T Middle;
    std::vector<Middle> middle_;

    typedef typename Search::template Table<Prob>::T Longest;
    Longest longest_;
};

struct ProbingSearch {
  typedef float Init;
  template <class Value> struct Table {
    typedef util::ProbingMap<uint64_t, Value, IdentityHash> T;
  };
};

struct SortedUniformSearch {
  typedef util::SortedUniformInit Init;
  template <class Value> struct Table {
    typedef util::SortedUniformMap<uint64_t, Value> T;
  };
};

} // namespace detail

// These must also be instantiated in the cc file.  
typedef detail::GenericVocabulary<detail::ProbingSearch> Vocabulary;
typedef detail::GenericModel<detail::ProbingSearch> Model;

typedef detail::GenericVocabulary<detail::SortedUniformSearch> SortedVocabulary;
typedef detail::GenericModel<detail::SortedUniformSearch> SortedModel;

} // namespace ngram
} // namespace lm

#endif // LM_NGRAM_H__

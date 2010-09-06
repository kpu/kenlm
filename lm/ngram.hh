#ifndef LM_NGRAM_H__
#define LM_NGRAM_H__

#include "lm/base.hh"
#include "util/probing_hash_table.hh"
#include "util/string_piece.hh"
#include "util/murmur_hash.hh"
#include "util/scoped.hh"

#include <boost/lexical_cast.hpp>
#include <boost/noncopyable.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/unordered_map.hpp>

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

namespace detail {
struct VocabularyFriend;
template <class Search> class GenericModel;
} // namespace detail

class Vocabulary : public base::Vocabulary {
  public:
    Vocabulary() {}

    WordIndex Index(const std::string &str) const {
      return Index(StringPiece(str));
    }

    WordIndex Index(const StringPiece &str) const {
      boost::unordered_map<StringPiece, WordIndex>::const_iterator i(ids_.find(str));
      return (__builtin_expect(i == ids_.end(), 0)) ? not_found_ : i->second;
    }

    bool Known(const StringPiece &str) const {
      return ids_.find(str) != ids_.end();
    }

    const char *Word(WordIndex index) const {
      return strings_[index].c_str();
    }

  protected:
    // friend interface for populating.
    friend struct detail::VocabularyFriend;

    void Reserve(size_t to) {
      strings_.reserve(to);
      ids_.rehash(to + 1);
    }

    WordIndex InsertUnique(std::string *word);

    void FinishedLoading();

  private:
    // TODO: optimize memory use here by using one giant buffer, preferably premade by a binary file format.
    boost::ptr_vector<std::string> strings_;

    // This proved faster than Boost's hash in speed trials: total load time Murmur 67090000, Boost 72210000
    struct MurmurHash : public std::unary_function<const StringPiece &, size_t> {
      size_t operator()(const StringPiece &key) const {
        return MurmurHash64A(reinterpret_cast<const void*>(key.data()), key.length(), 0);
      }
    };

    boost::unordered_map<StringPiece, WordIndex, MurmurHash> ids_;
};

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

struct Prob {
  float prob;
  void SetBackoff(float to) {
    throw FormatLoadException("Attempt to set backoff " + boost::lexical_cast<std::string>(to) + " for   an n-gram with longest order.");
  }
  void ZeroBackoff() {}
};
struct ProbBackoff : Prob {
  float backoff;
  void SetBackoff(float to) { backoff = to; }
  void ZeroBackoff() { backoff = 0.0; }
};

// Should return the same results as SRI except ln instead of log10
template <class Search> class GenericModel : public base::MiddleModel<GenericModel<Search>, State, Vocabulary> {
  private:
    typedef base::MiddleModel<GenericModel<Search>, State, Vocabulary> P;
  public:
    GenericModel(const char *file, Vocabulary &vocab, const typename Search::Init &init);

    Return WithLength(const State &in_state, const WordIndex new_word, State &out_state) const;

  private:
    void LoadFromARPA(util::FilePiece &f, Vocabulary &vocab, const std::vector<size_t> &counts);

    WordIndex not_found_;
    // memory_ is the backing store for unigram_, [middle_begin_, middle_end_), and longest_.  All of    these are pointers there.   
    util::scoped_mmap memory_;

    ProbBackoff *unigram_;

    typedef typename Search::template Table<ProbBackoff>::T Middle;
    std::vector<Middle> middle_;

    typedef typename Search::template Table<Prob>::T Longest;
    Longest longest_;
};

class ProbingSearch {
  private:
    // std::identity is an SGI extension :-(
    struct IdentityHash : public std::unary_function<uint64_t, size_t> {
      size_t operator()(uint64_t arg) const { return static_cast<size_t>(arg); }
    };

  public:
    typedef float Init;
    template <class Value> struct Table {
      typedef util::ProbingMap<uint64_t, Value, IdentityHash> T;
    };
};

} // namespace detail

typedef detail::GenericModel<detail::ProbingSearch> Model;

class Owner : boost::noncopyable {
  public:
    explicit Owner(const char *file_name) : model_(file_name, vocab_, 1.5) {}

    const Vocabulary &GetVocabulary() const { return model_.GetVocabulary(); }

    const Model &GetModel() const { return model_; }

  private:
    Vocabulary vocab_;
    const Model model_;
};

} // namespace ngram
} // namespace lm

#endif // LM_NGRAM_H__

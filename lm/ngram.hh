#ifndef LM_NGRAM_H__
#define LM_NGRAM_H__

#include "lm/base.hh"
#include "util/string_piece.hh"
#include "util/murmur_hash.hh"

#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/noncopyable.hpp>

#include <algorithm>
#include <vector>
#include <memory>

/* TODO: use vocab.hh and arpa_io.hh */

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

struct Return {
  float prob;
  unsigned char ngram_length;
};

namespace detail {
class ImplBase : boost::noncopyable {
  public:
    virtual ~ImplBase();
    virtual Return IncrementalScore(const State &in_state, const WordIndex new_word, State &out_state) const = 0;

  protected:
    ImplBase() {}
};
} // namespace detail

class Model : boost::noncopyable {
  public:
    typedef ::lm::ngram::State State;

    explicit Model(const char *file);
    ~Model();

    const State &BeginSentenceState() const { return begin_sentence_; }
    const State &NullContextState() const { return null_context_; }
    unsigned int Order() const { return order_; }
    const Vocabulary &GetVocabulary() const { return vocab_; }

    Return IncrementalScore(
        const State &in_state,
        const WordIndex new_word,
        State &out_state) const {
      return impl_->IncrementalScore(in_state, new_word, out_state);
    }

  private:
    unsigned int order_;
    State begin_sentence_, null_context_;

    Vocabulary vocab_;

    boost::scoped_ptr<detail::ImplBase> impl_;
};

// This just owns Model, which in turn owns Vocabulary.  Only reason this class
// exists is to provide the same interface as the other models.
class Owner : boost::noncopyable {
  public:
    explicit Owner(const char *file_name) : model_(file_name) {}

    const Vocabulary &GetVocabulary() const { return model_.GetVocabulary(); }

    const Model &GetModel() const { return model_; }

  private:
    const Model model_;
};

} // namespace ngram
} // namespace lm

#endif // LM_NGRAM_H__

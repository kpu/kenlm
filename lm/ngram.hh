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

class State {
  public:
    bool operator==(const State &other) const {
      if (ngram_length_ != other.ngram_length_) return false;
      for (const float *first = backoff_, *second = other.backoff_;
          first != backoff_ + ValidLength(); ++first, ++second) {
        // No arithmetic was performed on these values, so an exact comparison is justified.
        if (*first != *second) return false;
      }
      return true;
    }

    // The normal copy constructor isn't here to make this a POD.  You can also use this copy which might be faster.  
    void AlternateCopy(const State &other) {
      std::copy(other.backoff_, other.backoff_ + ValidLength(), backoff_);
    }

    unsigned char NGramLength() const { return ngram_length_; }

  private:
    template <class Search> friend class detail::GenericModel;
    friend class Model;
    friend size_t hash_value(const State &state);

    size_t ValidLength() const {
      return std::min<size_t>(static_cast<size_t>(ngram_length_), kMaxOrder - 1);
    }

    unsigned char ngram_length_;

    // The first min(ngram_length_, Model::order_ - 1) entries are valid backoff weights.
    // backoff_[0] is the backoff for unigrams.
    // The first min(ngram_length_, kMaxOrder - 1) entries must be copied and
    // may be used for hashing or equality.   
    float backoff_[kMaxOrder - 1];
};

inline size_t hash_value(const State &state) {
  size_t ret = 0;
  boost::hash_combine(ret, state.ngram_length_);
  for (const float *val = state.backoff_; val != state.backoff_ + state.ValidLength(); ++val) {
    boost::hash_combine(ret, *val);
  }
  return ret;
}

namespace detail {
class ImplBase : boost::noncopyable {
  public:
    virtual ~ImplBase();
    virtual float IncrementalScore(const State &in_state, const WordIndex *const words, const WordIndex *const words_end, State &out_state) const = 0;

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

    template <class ReverseHistoryIterator> float IncrementalScore(
        const State &in_state,
        ReverseHistoryIterator hist_iter,
        const WordIndex new_word,
        State &out_state) const {
      const unsigned int words_len = std::min<unsigned int>(in_state.NGramLength() + 1, order_);
      WordIndex words[words_len];
      WordIndex *const words_end = words + words_len;
      words[0] = new_word;
      WordIndex *dest = words + 1;
      for (; dest != words_end; ++dest, ++hist_iter) {
        *dest = *hist_iter;
      }
      // words is in reverse order.
      return impl_->IncrementalScore(in_state, words, words_end, out_state);
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

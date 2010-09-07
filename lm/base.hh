#ifndef LM_BASE_HH__
#define LM_BASE_HH__

#include "lm/word_index.hh"
#include "util/string_piece.hh"

#include <boost/noncopyable.hpp>

#include <string>

namespace lm {

struct Return {
  float prob;
  unsigned char ngram_length;
};

namespace base {

class Vocabulary : boost::noncopyable {
  public:
    virtual ~Vocabulary();

    WordIndex BeginSentence() const { return begin_sentence_; }
    WordIndex EndSentence() const { return end_sentence_; }
    WordIndex NotFound() const { return not_found_; }
    // Return start index of unused word assignments.
    WordIndex Available() const { return available_; }

    /* Most implementations allow StringPiece lookups and need only override
     * Index(StringPiece).  SRI requires null termination and overrides all
     * three methods.  
     */
    virtual WordIndex Index(const StringPiece &str) const = 0;
    virtual WordIndex Index(const std::string &str) const {
      return Index(StringPiece(str));
    }
    virtual WordIndex Index(const char *str) const {
      return Index(StringPiece(str));
    }

  protected:
    // Call SetSpecial afterward.  
    Vocabulary() {}

    Vocabulary(WordIndex begin_sentence, WordIndex end_sentence, WordIndex not_found, WordIndex available) {
      SetSpecial(begin_sentence, end_sentence, not_found, available);
    }

    void SetSpecial(WordIndex begin_sentence, WordIndex end_sentence, WordIndex not_found, WordIndex available);

    WordIndex begin_sentence_, end_sentence_, not_found_, available_;
};

template <class T, class U, class V> class MiddleModel;

class Model : boost::noncopyable {
  public:
    typedef ::lm::Return Return;

    virtual ~Model() {}

    size_t StateSize() const { return state_size_; }

    virtual Return WithLength(
        const void *in_state,
        const WordIndex new_word,
        void *out_state) const = 0;

    float Score(
        const void *in_state,
        const WordIndex new_word,
        void *out_state) const {
      return WithLength(in_state, new_word, out_state).prob;
    }

    const void *BeginSentenceMemory() const { return begin_sentence_memory_; }
    const void *NullContextMemory() const { return null_context_memory_; }

    unsigned char Order() const { return order_; }

    const Vocabulary &BaseVocabulary() const { return *base_vocab_; }

  protected:

  private:
    template <class T, class U, class V> friend class MiddleModel;
    explicit Model(size_t state_size) : state_size_(state_size) {}

    const size_t state_size_;
    const void *begin_sentence_memory_, *null_context_memory_;

    const Vocabulary *base_vocab_;

    unsigned char order_;
};

// Common model interface that depends on knowing the specific classes. 
// Curiously recurring template pattern.  
template <class Child, class StateT, class VocabularyT> class MiddleModel : public Model {
  public:
    typedef StateT State;
    typedef VocabularyT Vocabulary;
    Return WithLength(const void *in_state, const WordIndex new_word, void *out_state) const {
      return static_cast<const Child*>(this)->WithLength(
          *reinterpret_cast<const State*>(in_state),
          new_word,
          *reinterpret_cast<State*>(out_state));
    }

    float Score(const State &in_state, const WordIndex new_word, State &out_state) const {
      return static_cast<const Child*>(this)->WithLength(
          in_state,
          new_word,
          out_state).prob;
    }

    const State &BeginSentenceState() const { return begin_sentence_; }
    const State &NullContextState() const { return null_context_; }
    const Vocabulary &GetVocabulary() const { return *static_cast<const Vocabulary*>(&BaseVocabulary()); }

  protected:
    MiddleModel() : Model(sizeof(State)) {}

    virtual ~MiddleModel() {}

    // begin_sentence and null_context can disappear after.  vocab should stay.  
    void Init(const State &begin_sentence, const State &null_context, const Vocabulary &vocab, unsigned char order) {
      begin_sentence_ = begin_sentence;
      null_context_ = null_context;
      begin_sentence_memory_ = &begin_sentence_;
      null_context_memory_ = &null_context_;
      base_vocab_ = &vocab;
      order_ = order;
    }

  private:
    State begin_sentence_, null_context_;
};

} // mamespace base
} // namespace lm

#endif

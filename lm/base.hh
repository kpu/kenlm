#ifndef LM_BASE_HH__
#define LM_BASE_HH__

#include "lm/exception.hh"
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
		virtual ~Vocabulary() {}

		WordIndex BeginSentence() const { return begin_sentence_; }
		WordIndex EndSentence() const { return end_sentence_; }
		WordIndex NotFound() const { return not_found_; }
		// Return start index of unused word assignments.
		WordIndex Available() const { return available_; }

		virtual WordIndex Index(const std::string &str) const = 0;

	protected:
		// Delayed initialization of constant values.
		Vocabulary() : available_(0) {}

		void SetSpecial(WordIndex begin_sentence, WordIndex end_sentence, WordIndex not_found, WordIndex available) {
			begin_sentence_ = begin_sentence;
			end_sentence_ = end_sentence;
			not_found_ = not_found;
			available_ = available;
			if (begin_sentence_ == not_found_) throw BeginSentenceMissingException();
			if (end_sentence_ == not_found_) throw EndSentenceMissingException();
		}

		// Preferred: set constants at construction.
		Vocabulary(WordIndex begin_sentence, WordIndex end_sentence, WordIndex not_found, WordIndex available) {
			SetSpecial(begin_sentence, end_sentence, not_found, available);
		}

		WordIndex begin_sentence_, end_sentence_, not_found_, available_;
};

template <class T, class U, class V> class MiddleModel;

class Model : boost::noncopyable {
  public:
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

    const void *BeginSentenceMemory() const { return begin_memory_; }
    const void *NullContextMemory() const { return null_memory_; }

    unsigned int Order() { return order_; }

    const Vocabulary &BaseVocabulary() const { return base_vocab_; }

  protected:
    Model(size_t state_size, const Vocabulary &vocab) : state_size_(state_size), base_vocab_(vocab) {}

    unsigned int order_;

  private:
    template <class T, class U, class V> friend class MiddleModel;

    const size_t state_size_;
    const void *begin_memory_, *null_memory_;

    const Vocabulary &base_vocab_;
};

// Common model interface that depends on knowing the specific classes.  
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
    explicit MiddleModel(const Vocabulary &vocab) : Model(sizeof(State), vocab) {}

    void Init(const State &begin_state, const State &null_state) {
      begin_sentence_ = begin_state;
      null_context_ = null_state;
      begin_memory_ = &begin_sentence_;
      null_memory_ = &null_context_;
    }

  private:
    State begin_sentence_, null_context_;
};

} // mamespace base
} // namespace lm

#endif

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

    const void *BeginSentenceMemory() const { return begin_state_; }
    const void *NullContextMemory() const { return null_state_; }

    unsigned int Order() { return order_; }

    const Vocabulary &BaseVocabulary() const { return base_vocab_; }

  protected:
    Model(size_t state_size, const Vocabulary &vocab) : state_size_(state_size), base_vocab_(vocab) {}

    void Init(void *begin_state, void *null_state) {
      begin_state_ = begin_state;
      null_state_ = null_state;
    }
    unsigned int order_;

  private:
    const size_t state_size_;
    const void *begin_state_, *null_state_;

    const Vocabulary &base_vocab_;
};

} // mamespace base
} // namespace lm

#endif

#ifndef LM_BASE_HH__
#define LM_BASE_HH__

#include "lm/exception.hh"
#include "lm/word_index.hh"
#include "util/string_piece.hh"

#include <boost/noncopyable.hpp>

#include <string>

namespace lm {
namespace base {

class Vocabulary : boost::noncopyable {
	public:
		virtual ~Vocabulary() {}

		WordIndex BeginSentence() const { return begin_sentence_; }
		WordIndex EndSentence() const { return end_sentence_; }
		WordIndex NotFound() const { return not_found_; }
		// Return start index of unused word assignments.
		WordIndex Available() const { return available_; }

		// Warning: not threadsafe for SRI.
		virtual WordIndex Index(const std::string &str) const = 0;

		virtual const char *Word(WordIndex index) const = 0;

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

} // mamespace base

struct Return {
  float prob;
  unsigned char ngram_length;
};

} // namespace lm

#endif

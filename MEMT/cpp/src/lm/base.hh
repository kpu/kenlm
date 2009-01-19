#ifndef LM_BASE_HH__
#define LM_BASE_HH__

#include "lm/word_index.hh"

#include <exception>
#include <string>

namespace lm {
namespace base {

class Vocabulary {
	public:
		virtual ~Vocabulary() {}

		WordIndex BeginSentence() const { return begin_sentence_; }
		WordIndex EndSentence() const { return end_sentence_; }
		WordIndex NotFound() const { return not_found_; }
		// Return start index of unused word assignments.
		WordIndex Available() const { return available_; }

		// Warning: not threadsafe for SRI.
		virtual WordIndex Index(const std::string &str) const = 0;

	protected:
		Vocabulary(WordIndex begin_sentence, WordIndex end_sentence, WordIndex not_found, WordIndex available) :
			begin_sentence_(begin_sentence),
			end_sentence_(end_sentence),
			not_found_(not_found),
			available_(available) {}

		const WordIndex begin_sentence_, end_sentence_, not_found_, available_;
};

class LoadException : public std::exception {
        public:
                virtual ~LoadException() throw() {}

        protected:
                LoadException() throw() {}
};

} // mamespace base
} // namespace lm

#endif

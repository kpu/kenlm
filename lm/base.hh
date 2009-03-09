#ifndef LM_BASE_HH__
#define LM_BASE_HH__

#include "lm/word_index.hh"

#include <exception>
#include <string>

namespace lm {
namespace base {

// Not all LMs handle all errors with these.
class LoadException : public std::exception {
	public:
  	virtual ~LoadException() throw() {}

  protected:
    LoadException() throw() {}
};

class OpenFileLoadException : public LoadException {
	public:
		OpenFileLoadException(const char *name) throw() : name_(name) {
			what_ = "Error opening file ";
			what_ += name;
		}

		~OpenFileLoadException() throw() {}

		const char *what() const throw() { return what_.c_str(); }

	private:
		std::string name_;
		std::string what_;
};

class FormatLoadException : public LoadException {
	public:
		FormatLoadException(const char *complaint) throw() : what_(complaint) {}

		~FormatLoadException() throw() {}

		const char *what() const throw() { return what_.c_str(); }

	private:
		std::string what_;
};

class SpecialWordMissingException : public LoadException {
	public:
  	virtual ~SpecialWordMissingException() throw() {}

	protected:
		SpecialWordMissingException() throw() {}
};

class BeginSentenceMissingException : public SpecialWordMissingException {
	public:
		BeginSentenceMissingException() throw() {}

		~BeginSentenceMissingException() throw() {}

		const char *what() const throw() { return "Begin of sentence marker missing from vocabulary"; }
};

class EndSentenceMissingException : public SpecialWordMissingException {
	public:
		EndSentenceMissingException() throw() {}

		~EndSentenceMissingException() throw() {}

		const char *what() const throw() { return "End of sentence marker missing from vocabulary"; }
};

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

		virtual const char *Word(WordIndex index) const = 0;

	protected:
		// Delayed initialization of constant values.
		Vocabulary() {}

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
} // namespace lm

#endif

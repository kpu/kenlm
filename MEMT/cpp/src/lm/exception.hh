#ifndef LM_EXCEPTION_HH__
#define LM_EXCEPTION_HH__

#include "util/string_piece.hh"

#include <exception>
#include <string>

namespace lm {

class LoadException : public std::exception {
        public:
                virtual ~LoadException() throw() {}

        protected:
                LoadException() throw() {}
};

class VocabLoadException : public LoadException {
	public:
		virtual ~VocabLoadException() throw() {}

	protected:
		VocabLoadException() throw() {}
};

// Different words, same ids
class IDDuplicateVocabLoadException : public VocabLoadException {
        public:
                IDDuplicateVocabLoadException(unsigned int id, const StringPiece &existing, const StringPiece &replacement) throw();

                ~IDDuplicateVocabLoadException() throw() {}

                const char *what() const throw() { return what_.c_str(); }

        private:
                std::string what_;
};

// One word, two ids.
class WordDuplicateVocabLoadException : public VocabLoadException {
        public:
                WordDuplicateVocabLoadException(const StringPiece &word, unsigned int first, unsigned int second) throw();

                ~WordDuplicateVocabLoadException() throw() {}

                const char *what() const throw() { return what_.c_str(); }

        private:
                std::string what_;
};

} // namespace lm

#endif

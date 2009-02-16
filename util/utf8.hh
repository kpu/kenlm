/* Utilities for UTF-8.  */

#ifndef UTIL_UTF8_HH__
#define UTIL_UTF8_HH__

#include "util/string_piece.hh"

#include <exception>
#include <string>

namespace utf8 {

// This is what happens when you pass bad UTF8.  
class NotUTF8Exception : public std::exception {
	public:
		NotUTF8Exception(const StringPiece &original) throw();

		virtual ~NotUTF8Exception() throw() {}

		virtual const char *what() const throw() { return what_.c_str(); }

		// The string you passed.
		const std::string &Original() const { return original_; }

	private:
		const std::string original_;

		const std::string what_;
};

bool IsPunctuation(const StringPiece &text) throw(NotUTF8Exception);

/* TODO: Implement these in a way that doesn't botch Turkish.
void ToLower(const StringPiece &in, std::string &out) throw(NotUTF8Exception);

// Capitalize the first letter of in, if any.
void Capitalize(const StringPiece &in, std::string &out) throw(NotUTF8Exception);*/

} // namespace utf8

#endif

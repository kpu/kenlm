#include "lm/exception.hh"

#include<sstream>

namespace lm {

IDDuplicateVocabLoadException::IDDuplicateVocabLoadException(unsigned int id, const StringPiece &first, const StringPiece &second) throw() {
  std::ostringstream tmp;
  tmp << "Vocabulary id " << id << " is same for " << first << " and " << second;
  what_ = tmp.str();
}

WordDuplicateVocabLoadException::WordDuplicateVocabLoadException(const StringPiece &word, unsigned int first, unsigned int second) throw() {  
  std::ostringstream tmp;
  tmp << "Vocabulary word " << word << " has two ids: " << first << " and " << second;
  what_ = tmp.str();
}

FormatLoadException::FormatLoadException(const StringPiece &complaint, const StringPiece &context) throw() {
  what_.assign(complaint.data(), complaint.size());
	if (!context.empty()) {
		what_ += " at ";
		what_.append(context.data(), context.size());
	}
}

} // namespace lm

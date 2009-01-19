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

} // namespace lm

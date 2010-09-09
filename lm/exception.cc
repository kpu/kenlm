#include "lm/exception.hh"

#include<boost/lexical_cast.hpp>

#include<sstream>

#include<errno.h>
#include<stdio.h>

namespace lm {

NotFoundInVocabException::NotFoundInVocabException(const StringPiece &word) throw() : word_(word.data(), word.length()) {
  what_ = "Word '";
  what_ += word_;
  what_ += "' was not found in the vocabulary.";
}

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

AllocateMemoryLoadException::AllocateMemoryLoadException(size_t requested) throw() 
  : ErrnoException(std::string("Failed to allocate language model memory; asked for ") + boost::lexical_cast<std::string>(requested)) {}

FormatLoadException::FormatLoadException(const StringPiece &complaint, const StringPiece &context) throw() {
  what_.assign(complaint.data(), complaint.size());
  if (!context.empty()) {
    what_ += " at ";
    what_.append(context.data(), context.size());
  }
}

} // namespace lm

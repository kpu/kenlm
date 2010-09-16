#include "lm/exception.hh"

#include<errno.h>
#include<stdio.h>

namespace lm {

LoadException::LoadException() throw() {}
LoadException::~LoadException() throw() {}
VocabLoadException::VocabLoadException() throw() {}
VocabLoadException::~VocabLoadException() throw() {}

FormatLoadException::FormatLoadException() throw() {}
FormatLoadException::~FormatLoadException() throw() {}

SpecialWordMissingException::SpecialWordMissingException(StringPiece which) throw() {
  *this << "Missing special word " << which;
}
SpecialWordMissingException::~SpecialWordMissingException() throw() {}

} // namespace lm

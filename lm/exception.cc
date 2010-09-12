#include "lm/exception.hh"

#include<errno.h>
#include<stdio.h>

namespace lm {

LoadException::LoadException() throw() {}
LoadException::~LoadException() throw() {}
VocabLoadException::VocabLoadException() throw() {}
VocabLoadException::~VocabLoadException() throw() {}

AllocateMemoryLoadException::AllocateMemoryLoadException(size_t requested) throw() {
  *this << "Failed to allocate memory for " << requested << "bytes.";
}

AllocateMemoryLoadException::~AllocateMemoryLoadException() throw() {}

FormatLoadException::FormatLoadException() throw() {}
FormatLoadException::~FormatLoadException() throw() {}

SpecialWordMissingException::SpecialWordMissingException(StringPiece which) throw() {
  *this << "Missing special word " << which;
}
SpecialWordMissingException::~SpecialWordMissingException() throw() {}

} // namespace lm

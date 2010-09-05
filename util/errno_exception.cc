#include "util/errno_exception.hh"

#include <boost/lexical_cast.hpp>

#include <errno.h>
#include <stdio.h>

namespace util {

ErrnoException::ErrnoException(const StringPiece &problem) throw() : errno_(errno), what_(problem.data(), problem.size()) {
  if (errno_ < sys_nerr) {
    what_ += sys_errlist[errno_];
   } else {
    what_ += ": " + boost::lexical_cast<std::string>(errno_);
  }
}

ErrnoException::~ErrnoException() throw() {}

} // namespace util

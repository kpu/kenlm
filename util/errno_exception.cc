#include "util/errno_exception.hh"

#include <boost/lexical_cast.hpp>

#include <errno.h>
#include <stdio.h>

namespace util {

ErrnoException::ErrnoException(const StringPiece &problem) throw() : errno_(errno), what_(problem.data(), problem.size()) {
  char buf[200];
  buf[0] = 0;
#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE
  const char *add = buf;
  if (!strerror_r(errno, buf, 200)) {
#else
  const char *add = strerror_r(errno, buf, 200);
  if (add) {
#endif
    what_ += add;
  }
}

ErrnoException::~ErrnoException() throw() {}

} // namespace util

#include "util/exception.hh"

#include <errno.h>
#include <string.h>

namespace util {

Exception::Exception() throw() {}
Exception::~Exception() throw() {}

namespace {
// The XOPEN version.
const char *HandleStrerror(int ret, const char *buf) {
  if (!ret) return buf;
  return NULL;
}

// The GNU version.
const char *HandleStrerror(const char *ret, const char * /*buf*/) {
  return ret;
}
} // namespace

ErrnoException::ErrnoException() throw() : errno_(errno) {
  char buf[200];
  buf[0] = 0;
#ifdef sun
  const char *add = strerror(errno);
#else
  const char *add = HandleStrerror(strerror_r(errno, buf, 200), buf);
#endif

  if (add) {
    *this << add << ' ';
  }
}

ErrnoException::~ErrnoException() throw() {}

} // namespace util

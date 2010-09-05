#ifndef UTIL_ERRNO_EXCEPTION__
#define UTIL_ERRNO_EXCEPTION__

#include <exception>
#include <string>

#include "util/string_piece.hh"

namespace util {

class ErrnoException : public std::exception {
  public:
    explicit ErrnoException(const StringPiece &problem) throw();

    virtual ~ErrnoException() throw();

    virtual const char *what() const throw() { return what_.c_str(); }

    int Error() { return errno_; }

  private:
    int errno_;
    std::string what_;
};

} // namespace util

#endif // UTIL_ERRNO_EXCEPTION__

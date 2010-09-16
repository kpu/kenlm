#ifndef UTIL_EXCEPTION__
#define UTIL_EXCEPTION__

#include "util/string_piece.hh"

#include <exception>
#include <sstream>
#include <string>

namespace util {

class Exception : public std::exception {
  public:
    Exception() throw();
    virtual ~Exception() throw();

    const char *what() const throw() { return what_.c_str(); }

    // This helps restrict operator<< defined below.  
    template <class T> struct ExceptionTag {
      typedef T Identity;
    };

    std::string &Str() {
      return what_;
    }

  protected:
    std::string what_;
};

/* This implements the normal operator<< for Exception and all its children. 
 * SNIFAE means it only applies to Exception.  Think of this as an ersatz
 * boost::enable_if.  
 */
template <class Except, class Data> typename Except::template ExceptionTag<Except&>::Identity operator<<(Except &e, const Data &data) {
  // Argh I had a stringstream in the exception, but the only way to get the string is by calling str().  But that's a temporary string, so virtual const char *what() const can't actually return it.  
  std::stringstream stream;
  stream << data;
  e.Str() += stream.str();
  return e;
}
template <class Except> typename Except::template ExceptionTag<Except&>::Identity operator<<(Except &e, const char *data) {
  e.Str() += data;
  return e;
}
template <class Except> typename Except::template ExceptionTag<Except&>::Identity operator<<(Except &e, const std::string &data) {
  e.Str() += data;
  return e;
}
template <class Except> typename Except::template ExceptionTag<Except&>::Identity operator<<(Except &e, const StringPiece &str) {
  e.Str().append(str.data(), str.length());
  return e;
}

#define UTIL_THROW(Exception, Modify) { Exception UTIL_e; {UTIL_e << Modify;} throw UTIL_e; }

class ErrnoException : public Exception {
  public:
    ErrnoException() throw();

    virtual ~ErrnoException() throw();

    int Error() { return errno_; }

  private:
    int errno_;
};

} // namespace util

#endif // UTIL_EXCEPTION__

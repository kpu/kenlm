#ifndef UTIL_ERRNO_EXCEPTION__
#define UTIL_ERRNO_EXCEPTION__

#include <exception>
#include <sstream>

namespace util {

class Exception : public std::exception {
  public:
    Exception() throw();
    virtual ~Exception() throw();
    Exception(const Exception &other) throw();
    Exception &operator=(const Exception &other) throw();

    virtual const char *what() const throw();

    std::stringstream &str() { return stream_; }

    // This helps restrict operator<< defined below.  
    template <class T> struct ExceptionTag {
      typedef T Identity;
    };

  protected:
    std::stringstream stream_;
};

/* This implements the normal operator<< for Exception and all its children. 
 * SNIFAE means it only applies to Exception.  Think of this as an ersatz
 * boost::enable_if.  
 */
template <class Except, class Data> typename Except::template ExceptionTag<Except&>::Identity operator<<(Except &e, const Data &data) {
  e.str() << data;
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

#endif // UTIL_ERRNO_EXCEPTION__

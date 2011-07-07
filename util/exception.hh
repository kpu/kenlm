#ifndef UTIL_EXCEPTION__
#define UTIL_EXCEPTION__

#include <exception>
#include <sstream>
#include <string>

namespace util {

template <class Except, class Data> typename Except::template ExceptionTag<Except&>::Identity operator<<(Except &e, const Data &data);

class Exception : public std::exception {
  public:
    Exception() throw();
    virtual ~Exception() throw();

    Exception(const Exception &from);
    Exception &operator=(const Exception &from);

    // Not threadsafe, but probably doesn't matter.  FWIW, Boost's exception guidance implies that what() isn't threadsafe.  
    const char *what() const throw();

    // For use by the UTIL_THROW macro.
    void SetLocation(const char *file, unsigned int line, const char *child_name, const char *condition = NULL);

  private:
    template <class Except, class Data> friend typename Except::template ExceptionTag<Except&>::Identity operator<<(Except &e, const Data &data);

    // This helps restrict operator<< defined below.  
    template <class T> struct ExceptionTag {
      typedef T Identity;
    };

    std::stringstream stream_;
    mutable std::string text_;
};

/* This implements the normal operator<< for Exception and all its children. 
 * SNIFAE means it only applies to Exception.  Think of this as an ersatz
 * boost::enable_if.  
 */
template <class Except, class Data> typename Except::template ExceptionTag<Except&>::Identity operator<<(Except &e, const Data &data) {
  e.stream_ << data;
  return e;
}

// do .. while kludge to swallow ; http://gcc.gnu.org/onlinedocs/cpp/Swallowing-the-Semicolon.html
// Modify is not surrounded with () so that it can use << internally.  
#define UTIL_THROW(Exception, Modify) do { \
  Exception UTIL_e; \
  UTIL_e.SetLocation(__FILE__, __LINE__, #Exception); \
  UTIL_e << Modify; \
  throw UTIL_e; \
} while (0)

#define UTIL_THROW_IF(Condition, Exception, Modify) do { \
  if (Condition) { \
    Exception UTIL_e; \
    UTIL_e.SetLocation(__FILE__, __LINE__, #Exception, #Condition); \
    UTIL_e << Modify; \
    throw UTIL_e; \
  } \
} while (0)

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

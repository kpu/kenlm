#ifndef UTIL_EXCEPTION__
#define UTIL_EXCEPTION__

#include <exception>
#include <limits>
#include <sstream>
#include <string>

#include <stdint.h>

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

    // For use by the UTIL_THROW macros.  
    void SetLocation(
        const char *file,
        unsigned int line,
        const char *func,
        const char *child_name,
        const char *condition);

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
 * SFINAE means it only applies to Exception.  Think of this as an ersatz
 * boost::enable_if.  
 */
template <class Except, class Data> typename Except::template ExceptionTag<Except&>::Identity operator<<(Except &e, const Data &data) {
  e.stream_ << data;
  return e;
}

#ifdef __GNUC__
#define UTIL_FUNC_NAME __PRETTY_FUNCTION__
#else
#ifdef _WIN32
#define UTIL_FUNC_NAME __FUNCTION__
#else
#define UTIL_FUNC_NAME NULL
#endif
#endif

/* Create an instance of Exception, add the message Modify, and throw it.
 * Modify is appended to the what() message and can contain << for ostream
 * operations.  
 *
 * do .. while kludge to swallow trailing ; character
 * http://gcc.gnu.org/onlinedocs/cpp/Swallowing-the-Semicolon.html .  
 * Arg can be a constructor argument to the exception.
 */
#define UTIL_THROW_BACKEND(Condition, Exception, Arg, Modify) do { \
  Exception UTIL_e Arg; \
  UTIL_e.SetLocation(__FILE__, __LINE__, UTIL_FUNC_NAME, #Exception, Condition); \
  UTIL_e << Modify; \
  throw UTIL_e; \
} while (0)

#define UTIL_THROW_ARG(Exception, Arg, Modify) \
  UTIL_THROW_BACKEND(NULL, Exception, Arg, Modify)

#define UTIL_THROW(Exception, Modify) \
  UTIL_THROW_BACKEND(NULL, Exception, , Modify);

#if __GNUC__ >= 3
#define UTIL_UNLIKELY(x) __builtin_expect (!!(x), 0)
#else
#define UTIL_UNLIKELY(x) (x)
#endif

#define UTIL_THROW_IF_ARG(Condition, Exception, Arg, Modify) do { \
  if (UTIL_UNLIKELY(Condition)) { \
    UTIL_THROW_BACKEND(#Condition, Exception, Arg, Modify); \
  } \
} while (0)

#define UTIL_THROW_IF(Condition, Exception, Modify) \
  UTIL_THROW_IF_ARG(Condition, Exception, , Modify)

#define UTIL_THROW_IF2(Condition, Modify) \
  UTIL_THROW_IF_ARG(Condition, util::Exception, , Modify)

// Exception that records errno and adds it to the message.
class ErrnoException : public Exception {
  public:
    ErrnoException() throw();

    virtual ~ErrnoException() throw();

    int Error() const throw() { return errno_; }

  private:
    int errno_;
};

// file wasn't there, or couldn't be open for some reason
class FileOpenException : public Exception {
  public:
	FileOpenException() throw() {}
    ~FileOpenException() throw() {}
};

// Utilities for overflow checking.  
class OverflowException : public Exception {
  public:
    OverflowException() throw();
    ~OverflowException() throw();
};

template <unsigned len> inline std::size_t CheckOverflowInternal(uint64_t value) {
  UTIL_THROW_IF(value > static_cast<uint64_t>(std::numeric_limits<std::size_t>::max()), OverflowException, "Integer overflow detected.  This model is too big for 32-bit code.");
  return value;
}

template <> inline std::size_t CheckOverflowInternal<8>(uint64_t value) {
  return value;
}

inline std::size_t CheckOverflow(uint64_t value) {
  return CheckOverflowInternal<sizeof(std::size_t)>(value);
}

} // namespace util

#endif // UTIL_EXCEPTION__

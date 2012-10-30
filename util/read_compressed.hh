#ifndef UTIL_READ_COMPRESSED__
#define UTIL_READ_COMPRESSED__

#include "util/exception.hh"

#include <cstddef>

namespace util {

class CompressedException : public Exception {
  public:
    CompressedException() throw();
    ~CompressedException() throw();
};

class GZException : public CompressedException {
  public:
    GZException() throw();
    ~GZException() throw();
};

class ReadCompressed {
  public:
    // Takes ownership of fd.   
    static ReadCompressed *Open(int fd);

    virtual ~ReadCompressed();

    virtual std::size_t Read(void *to, std::size_t amount) = 0;

  protected:
    ReadCompressed();
};

} // namespace util

#endif // UTiL_READ_COMPRESSED__

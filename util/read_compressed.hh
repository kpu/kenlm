#ifndef UTIL_READ_COMPRESSED__
#define UTIL_READ_COMPRESSED__

#include "util/exception.hh"
#include "util/scoped.hh"

#include <cstddef>

namespace util {

class CompressedException : public Exception {
  public:
    CompressedException() throw();
    virtual ~CompressedException() throw();
};

class GZException : public CompressedException {
  public:
    GZException() throw();
    ~GZException() throw();
};

class BZException : public CompressedException {
  public:
    BZException() throw();
    ~BZException() throw();
};

class ReadBase;

class ReadCompressed {
  public:
    // Takes ownership of fd.   
    explicit ReadCompressed(int fd);

    ~ReadCompressed();

    std::size_t Read(void *to, std::size_t amount);

  private:
    scoped_ptr<ReadBase> internal_;

    // No copying.  
    ReadCompressed(const ReadCompressed &);
    void operator=(const ReadCompressed &);
};

} // namespace util

#endif // UTiL_READ_COMPRESSED__

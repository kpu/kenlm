#ifndef UTIL_READ_COMPRESSED__
#define UTIL_READ_COMPRESSED__

#include "util/exception.hh"
#include "util/scoped.hh"

#include <cstddef>

#include <stdint.h>

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

class XZException : public CompressedException {
  public:
    XZException() throw();
    ~XZException() throw();
};

class ReadBase;

class ReadCompressed {
  public:
    static const std::size_t kMagicSize = 6;
    // Must have at least kMagicSize bytes.  
    static bool DetectCompressedMagic(const void *from);

    // Takes ownership of fd.   
    explicit ReadCompressed(int fd);

    // Try to avoid using this.  Use the fd instead.
    // There is no decompression support for istreams.
    explicit ReadCompressed(std::istream &in);

    // Must call Reset later.
    ReadCompressed();

    ~ReadCompressed();

    // Takes ownership of fd.  
    void Reset(int fd);

    // Same advice as the constructor.
    void Reset(std::istream &in);

    std::size_t Read(void *to, std::size_t amount);

    uint64_t RawAmount() const { return raw_amount_; }

  private:
    friend class ReadBase;

    scoped_ptr<ReadBase> internal_;

    uint64_t raw_amount_;

    // No copying.  
    ReadCompressed(const ReadCompressed &);
    void operator=(const ReadCompressed &);
};

} // namespace util

#endif // UTIL_READ_COMPRESSED__

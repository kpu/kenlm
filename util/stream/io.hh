#ifndef UTIL_STREAM_IO__
#define UTIL_STREAM_IO__

#include "util/exception.hh"
#include "util/file.hh"
#include "util/stream/chain.hh"

#include <cstddef>

#include <assert.h>
#include <stdint.h>

namespace util {
namespace stream {

class ReadSizeException : public util::Exception {
  public:
    ReadSizeException() throw();
    ~ReadSizeException() throw();
};

class Read {
  public:
    explicit Read(int fd) : file_(fd) {}

    void Run(const ChainPosition &position); 

  private:
    int file_;
};

class Write {
  public:
    explicit Write(int fd) : file_(fd) {}

    void Run(const ChainPosition &position) {
      for (Link link(position); link; ++link) {
        util::WriteOrThrow(file_, link->Get(), link->ValidSize());
      }
    }

  private:
    int file_;
};

} // namespace stream
} // namespace util
#endif // UTIL_STREAM_IO__

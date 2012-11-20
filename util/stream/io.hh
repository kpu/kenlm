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

class ReadThread : public LinkThread<ReadThread> {
  public:
    ReadThread(Chain &chain, int fd) :
        LinkThread<ReadThread>(chain),
        file_(fd),
        entry_size_(chain.EntrySize()),
        block_size_(chain.BlockSize()) {}

    bool Process(Block &block);

  private:
    scoped_fd file_;
    const std::size_t entry_size_, block_size_;
};

class WriteThread : public LinkThread<WriteThread> {
  public:
    WriteThread(Chain &chain, int fd) :
        LinkThread<WriteThread>(chain),
        file_(fd) {}

    bool Process(Block &block) {
      util::WriteOrThrow(file_.get(), block.Get(), block.ValidSize());
      return true;
    }

  private:
    scoped_fd file_;
};

} // namespace stream
} // namespace util
#endif // UTIL_STREAM_IO__

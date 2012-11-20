#ifndef UTIL_STREAM_BLOCK__
#define UTIL_STREAM_BLOCK__

#include <cstddef>

namespace util {
namespace stream {

class Block {
  public:
    Block() : mem_(NULL), valid_size_(0) {}

    Block(void *mem, std::size_t size) : mem_(mem), valid_size_(size) {}

    void SetValidSize(std::size_t to) { valid_size_ = to; }
    // Read might fill in less than Allocated at EOF.   
    std::size_t ValidSize() const { return valid_size_; }

    void *Get() { return mem_; }
    const void *Get() const { return mem_; }

    operator bool() const { return valid_size_ != 0; }
    bool operator!() const { return !valid_size_; }

  private:
    void *mem_;
    std::size_t valid_size_;
};

} // namespace stream
} // namespace util

#endif // UTIL_STREAM_BLOCK__

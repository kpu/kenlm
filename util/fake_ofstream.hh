/* Like std::ofstream but without being incredibly slow.  Backed by a raw fd.
 * Does not support many data types.  Currently, it's targeted at writing ARPA
 * files quickly.
 */
#ifndef UTIL_FAKE_OFSTREAM_H
#define UTIL_FAKE_OFSTREAM_H

#include "util/file.hh"
#include "util/float_to_string.hh"
#include "util/integer_to_string.hh"
#include "util/scoped.hh"
#include "util/string_piece.hh"

#include <cassert>
#include <cstring>

#include <stdint.h>

namespace util {
class FakeOFStream {
  private:
    // Maximum over all ToString operations.
    static const std::size_t kMinBuf = 20;
  public:
    // Does not take ownership of out.
    // Allows default constructor, but must call SetFD.
    explicit FakeOFStream(int out = -1, std::size_t buffer_size = 1048576)
      : buf_(util::MallocOrThrow(std::max(buffer_size, kMinBuf))),
        current_(static_cast<char*>(buf_.get())),
        end_(current_ + std::max(buffer_size, kMinBuf)),
        fd_(out) {}

    ~FakeOFStream() {
      // Could have called Finish already
      Flush();
    }

    void SetFD(int to) {
      Flush();
      fd_ = to;
    }

    FakeOFStream &Write(const void *data, std::size_t length) {
      if (UTIL_LIKELY(current_ + length <= end_)) {
        std::memcpy(current_, data, length);
        current_ += length;
        return *this;
      }
      Flush();
      if (current_ + length <= end_) {
        std::memcpy(current_, data, length);
        current_ += length;
      } else {
        util::WriteOrThrow(fd_, data, length);
      }
      return *this;
    }

    FakeOFStream &operator<<(StringPiece str) {
      return Write(str.data(), str.size());
    }

    FakeOFStream &operator<<(float value) { return CallToString(value); }
    FakeOFStream &operator<<(double value) { return CallToString(value); }
    FakeOFStream &operator<<(uint64_t value) { return CallToString(value); }
    FakeOFStream &operator<<(int64_t value) { return CallToString(value); }
    FakeOFStream &operator<<(uint32_t value) { return CallToString(value); }
    FakeOFStream &operator<<(int32_t value) { return CallToString(value); }
    FakeOFStream &operator<<(uint16_t value) { return CallToString(value); }
    FakeOFStream &operator<<(int16_t value) { return CallToString(value); }
    FakeOFStream &operator<<(bool value) { return CallToString(value); }

    FakeOFStream &operator<<(char c) {
      EnsureRemaining(1);
      *current_++ = c;
      return *this;
    }

    FakeOFStream &operator<<(unsigned char c) {
      EnsureRemaining(1);
      *current_++ = static_cast<char>(c);
      return *this;
    }

    // Note this does not sync.
    void Flush() {
      if (current_ != buf_.get()) {
        util::WriteOrThrow(fd_, buf_.get(), current_ - (char*)buf_.get());
        current_ = static_cast<char*>(buf_.get());
      }
    }

    // Not necessary, but does assure the data is cleared.
    void Finish() {
      Flush();
      buf_.reset();
      current_ = NULL;
      util::FSyncOrThrow(fd_);
    }

  private:
    template <class T> FakeOFStream &CallToString(const T value) {
      EnsureRemaining(ToStringBuf<T>::kBytes);
      current_ = ToString(value, current_);
      assert(current_ <= end_);
      return *this;
    }

    void EnsureRemaining(std::size_t amount) {
      if (UTIL_UNLIKELY(current_ + amount > end_)) {
        Flush();
        assert(current_ + amount <= end_);
      }
    }

    util::scoped_malloc buf_;
    char *current_, *end_;

    int fd_;
};

} // namespace

#endif

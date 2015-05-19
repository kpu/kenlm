/* Like std::ofstream but without being incredibly slow.  Backed by a raw fd.
 * Supports most of the built-in types except for void* and long double.
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
  public:
    // Maximum over all ToString operations.
    // static const std::size_t kMinBuf = 20;
    // This was causing compile failures in debug, so now 20 is written directly.
    //
    // Does not take ownership of out.
    // Allows default constructor, but must call SetFD.
    explicit FakeOFStream(int out = -1, std::size_t buffer_size = 1048576)
      : buf_(util::MallocOrThrow(std::max(buffer_size, (size_t)20))),
        current_(static_cast<char*>(buf_.get())),
        end_(current_ + std::max(buffer_size, (size_t)20)),
        fd_(out) {}

    ~FakeOFStream() {
      // Could have called Finish already
      flush();
    }

    void SetFD(int to) {
      flush();
      fd_ = to;
    }

    FakeOFStream &write(const void *data, std::size_t length) {
      if (UTIL_LIKELY(current_ + length <= end_)) {
        std::memcpy(current_, data, length);
        current_ += length;
        return *this;
      }
      flush();
      if (current_ + length <= end_) {
        std::memcpy(current_, data, length);
        current_ += length;
      } else {
        util::WriteOrThrow(fd_, data, length);
      }
      return *this;
    }

    // This also covers std::string and char*
    FakeOFStream &operator<<(StringPiece str) {
      return write(str.data(), str.size());
    }

    // For anything with ToStringBuf<T>::kBytes, define operator<< using ToString.
    // This includes uint64_t, int64_t, uint32_t, int32_t, uint16_t, int16_t,
    // float, double
  private:
    template <int Arg> struct EnableIfKludge {
      typedef FakeOFStream type;
    };
  public:
    template <class T> typename EnableIfKludge<ToStringBuf<T>::kBytes>::type &operator<<(const T value) {
      EnsureRemaining(ToStringBuf<T>::kBytes);
      current_ = ToString(value, current_);
      assert(current_ <= end_);
      return *this;
    }

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

    /* clang on OS X appears to consider std::size_t aka unsigned long distinct
     * from uint64_t.  So this function makes clang work.  gcc considers
     * uint64_t and std::size_t the same (on 64-bit) so this isn't necessary.
     * But it does no harm since gcc sees it as a specialization of the
     * EnableIfKludge template.
     * Also, delegating to *this << static_cast<uint64_t>(value) would loop
     * indefinitely on gcc.
     */
    FakeOFStream &operator<<(std::size_t value) {
      EnsureRemaining(ToStringBuf<uint64_t>::kBytes);
      current_ = ToString(static_cast<uint64_t>(value), current_);
      return *this;
    }

    // Note this does not sync.
    void flush() {
      if (current_ != buf_.get()) {
        util::WriteOrThrow(fd_, buf_.get(), current_ - (char*)buf_.get());
        current_ = static_cast<char*>(buf_.get());
      }
    }

    // Not necessary, but does assure the data is cleared.
    void Finish() {
      flush();
      buf_.reset();
      current_ = NULL;
      util::FSyncOrThrow(fd_);
    }

  private:
    void EnsureRemaining(std::size_t amount) {
      if (UTIL_UNLIKELY(current_ + amount > end_)) {
        flush();
        assert(current_ + amount <= end_);
      }
    }

    util::scoped_malloc buf_;
    char *current_, *end_;

    int fd_;
};

} // namespace

#endif

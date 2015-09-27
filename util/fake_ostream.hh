#ifndef UTIL_FAKE_OSTREAM_H
#define UTIL_FAKE_OSTREAM_H

#include "util/float_to_string.hh"
#include "util/integer_to_string.hh"
#include "util/string_piece.hh"

#include <cassert>
#include <string>

#include <stdint.h>

namespace util {

/* Like std::ostream but without being incredibly slow.
 * Supports most of the built-in types except for void* and long double.
 * 
 * The FakeOStream class is intended to be inherited from.  The inherting class
 * should provide:
 * public:
 *   Derived &flush();
 *   Derived &write(const void *data, std::size_t length);
 * 
 * private: or protected:
 *   friend class FakeOStream;
 *   char *Ensure(std::size_t amount);
 *   void AdvanceTo(char *to);
 *
 * The Ensure function makes enough space for an in-place write and returns
 * where to write.  The AdvanceTo function happens after the write, saying how
 * much was actually written.
 * 
 * Precondition:
 * amount <= kToStringMaxBytes for in-place writes.
 */
template <class Derived> class FakeOStream {
  public:
    FakeOStream() {}

    // This also covers std::string and char*
    Derived &operator<<(StringPiece str) {
      return C().write(str.data(), str.size());
    }

    // For anything with ToStringBuf<T>::kBytes, define operator<< using ToString.
    // This includes uint64_t, int64_t, uint32_t, int32_t, uint16_t, int16_t,
    // float, double
  private:
    template <int Arg> struct EnableIfKludge {
      typedef Derived type;
    };
  public:
    template <class T> typename EnableIfKludge<ToStringBuf<T>::kBytes>::type &operator<<(const T value) {
      C().AdvanceTo(ToString(value, C().Ensure(ToStringBuf<T>::kBytes)));
      return C();
    }

    /* clang on OS X appears to consider std::size_t aka unsigned long distinct
     * from uint64_t.  So this function makes clang work.  gcc considers
     * uint64_t and std::size_t the same (on 64-bit) so this isn't necessary.
     * But it does no harm since gcc sees it as a specialization of the
     * EnableIfKludge template.
     * Also, delegating to *this << static_cast<uint64_t>(value) would loop
     * indefinitely on gcc.
     */
    Derived &operator<<(std::size_t value) {
      C().AdvanceTo(ToString(static_cast<uint64_t>(value), C().Ensure(ToStringBuf<uint64_t>::kBytes)));
      return C();
    }

    Derived &operator<<(char val) {
      char *c = C().Ensure(1);
      *c = val;
      C().AdvanceTo(++c);
      return C();
    }

    Derived &operator<<(signed char val) {
      return *this << static_cast<char>(val);
    }

    Derived &operator<<(unsigned char val) {
      return *this << static_cast<char>(val);
    }

    Derived &put(char c) {
      return *this << c;
    }

  private:
    Derived &C() {
      return *static_cast<Derived*>(this);
    }

    const Derived &C() const {
      return *static_cast<const Derived*>(this);
    }
};

class FakeSStream : public FakeOStream<FakeSStream> {
  public:
    explicit FakeSStream(std::string &out)
      : out_(out) {}

    FakeSStream &flush() { return *this; }

    FakeSStream &write(const void *data, std::size_t length) {
      out_.append(static_cast<const char*>(data), length);
      return *this;
    }

  protected:
    friend class FakeOStream;
    char *Ensure(std::size_t amount) {
      std::size_t current = out_.size();
      out_.resize(out_.size() + amount);
      return &out_[current];
    }

    void AdvanceTo(char *to) {
      assert(to <= &*out_.end());
      assert(to >= &*out_.begin());
      out_.resize(to - &*out_.begin());
    }

  private:
    std::string &out_;
};

} // namespace

#endif // UTIL_FAKE_OSTREAM_H

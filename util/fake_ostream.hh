#ifndef UTIL_FAKE_OSTREAM_H
#define UTIL_FAKE_OSTREAM_H

#include "util/float_to_string.hh"
#include "util/integer_to_string.hh"
#include "util/string_piece.hh"

#include <cassert>
#include <limits>

#include <stdint.h>

namespace util {

/* Like std::ostream but without being incredibly slow.
 * Supports most of the built-in types except for long double.
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
      return CallToString(value);
    }

    /* clang on OS X appears to consider std::size_t aka unsigned long distinct
     * from uint64_t.  So this function makes clang work.  gcc considers
     * uint64_t and std::size_t the same (on 64-bit) so this isn't necessary.
     * But it does no harm since gcc sees it as a specialization of the
     * EnableIfKludge template.
     * Also, delegating to *this << static_cast<uint64_t>(value) would loop
     * indefinitely on gcc.
     */
    Derived &operator<<(std::size_t value) { return CoerceToString(value); }

    // union types will map to int, but don't pass the template magic above in gcc.
    Derived &operator<<(int value) { return CoerceToString(value); }

    // gcc considers these distinct from uint64_t
    Derived &operator<<(unsigned long long value) { return CoerceToString(value); }
    Derived &operator<<(signed long long value) { return CoerceToString(value); }

    // Character types that get copied as bytes instead of displayed as integers.
    Derived &operator<<(char val) { return put(val); }
    Derived &operator<<(signed char val) { return put(static_cast<char>(val)); }
    Derived &operator<<(unsigned char val) { return put(static_cast<char>(val)); }

    // This is here to catch all the other pointer types.
    Derived &operator<<(const void *value) { return CallToString(value); }
    // This is here because the above line also catches const char*.
    Derived &operator<<(const char *value) { return *this << StringPiece(value); }
    Derived &operator<<(char *value) { return *this << StringPiece(value); }

    Derived &put(char val) {
      char *c = C().Ensure(1);
      *c = val;
      C().AdvanceTo(++c);
      return C();
    }

    char widen(char val) const { return val; }

  private:
    // References to derived class for convenience.
    Derived &C() {
      return *static_cast<Derived*>(this);
    }

    const Derived &C() const {
      return *static_cast<const Derived*>(this);
    }

    template <class From, unsigned Length = sizeof(From), bool Signed = std::numeric_limits<From>::is_signed> struct Coerce {};

    template <class From> struct Coerce<From, 2, false> { typedef uint16_t To; };
    template <class From> struct Coerce<From, 4, false> { typedef uint32_t To; };
    template <class From> struct Coerce<From, 8, false> { typedef uint64_t To; };

    template <class From> struct Coerce<From, 2, true> { typedef int16_t To; };
    template <class From> struct Coerce<From, 4, true> { typedef int32_t To; };
    template <class From> struct Coerce<From, 8, true> { typedef int64_t To; };

    template <class From> Derived &CoerceToString(const From value) {
      return CallToString(static_cast<typename Coerce<From>::To>(value));
    }

    // This is separate to prevent an infinite loop if the compiler considers
    // types the same (i.e. gcc std::size_t and uint64_t or uint32_t).
    template <class T> Derived &CallToString(const T value) {
      C().AdvanceTo(ToString(value, C().Ensure(ToStringBuf<T>::kBytes)));
      return C();
    }
};

} // namespace

#endif // UTIL_FAKE_OSTREAM_H

#ifndef UTIL_SIZED_ITERATOR_H
#define UTIL_SIZED_ITERATOR_H

#include "util/proxy_iterator.hh"

#include <algorithm>
#include <functional>
#include <string>

#include <stdint.h>
#include <cstring>

namespace util {

class SizedInnerIterator {
  public:
    SizedInnerIterator() {}

    SizedInnerIterator(void *ptr, std::size_t size) : ptr_(static_cast<uint8_t*>(ptr)), size_(size) {}

    bool operator==(const SizedInnerIterator &other) const {
      return ptr_ == other.ptr_;
    }
    bool operator<(const SizedInnerIterator &other) const {
      return ptr_ < other.ptr_;
    }
    SizedInnerIterator &operator+=(std::ptrdiff_t amount) {
      ptr_ += amount * size_;
      return *this;
    }
    std::ptrdiff_t operator-(const SizedInnerIterator &other) const {
      return (ptr_ - other.ptr_) / size_;
    }

    const void *Data() const { return ptr_; }
    void *Data() { return ptr_; }
    std::size_t EntrySize() const { return size_; }

    friend void swap(SizedInnerIterator &first, SizedInnerIterator &second) {
      std::swap(first.ptr_, second.ptr_);
      std::swap(first.size_, second.size_);
    }

  private:
    uint8_t *ptr_;
    std::size_t size_;
};

class SizedProxy {
  public:
    SizedProxy() {}

    SizedProxy(void *ptr, std::size_t size) : inner_(ptr, size) {}

    operator std::string() const {
      return std::string(reinterpret_cast<const char*>(inner_.Data()), inner_.EntrySize());
    }

    SizedProxy &operator=(const SizedProxy &from) {
      memcpy(inner_.Data(), from.inner_.Data(), inner_.EntrySize());
      return *this;
    }

    SizedProxy &operator=(const std::string &from) {
      memcpy(inner_.Data(), from.data(), inner_.EntrySize());
      return *this;
    }

    const void *Data() const { return inner_.Data(); }
    void *Data() { return inner_.Data(); }

    friend void swap(SizedProxy first, SizedProxy second) {
      std::swap_ranges(
          static_cast<char*>(first.inner_.Data()),
          static_cast<char*>(first.inner_.Data()) + first.inner_.EntrySize(),
          static_cast<char*>(second.inner_.Data()));
    }

  private:
    friend class util::ProxyIterator<SizedProxy>;

    typedef std::string value_type;

    typedef SizedInnerIterator InnerIterator;

    InnerIterator &Inner() { return inner_; }
    const InnerIterator &Inner() const { return inner_; }
    InnerIterator inner_;
};

typedef ProxyIterator<SizedProxy> SizedIterator;

inline SizedIterator SizedIt(void *ptr, std::size_t size) { return SizedIterator(SizedProxy(ptr, size)); }

// Useful wrapper for a comparison function i.e. sort.
template <class Delegate, class Proxy = SizedProxy> class SizedCompare : public std::binary_function<const Proxy &, const Proxy &, bool> {
  public:
    explicit SizedCompare(const Delegate &delegate = Delegate()) : delegate_(delegate) {}

    bool operator()(const Proxy &first, const Proxy &second) const {
      return delegate_(first.Data(), second.Data());
    }
    bool operator()(const Proxy &first, const std::string &second) const {
      return delegate_(first.Data(), second.data());
    }
    bool operator()(const std::string &first, const Proxy &second) const {
      return delegate_(first.data(), second.Data());
    }
    bool operator()(const std::string &first, const std::string &second) const {
      return delegate_(first.data(), second.data());
    }

    const Delegate &GetDelegate() const { return delegate_; }

  private:
    const Delegate delegate_;
};

} // namespace util
#endif // UTIL_SIZED_ITERATOR_H

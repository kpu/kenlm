#ifndef UTIL_FIXED_ARRAY_H
#define UTIL_FIXED_ARRAY_H

// Ever want an array of things by they don't have a default constructor or are
// non-copyable?  FixedArray allows constructing one at a time.
#include "util/scoped.hh"

#include <cstddef>

#include <assert.h>
#include <stdlib.h>

namespace util {

template <class T> class FixedArray {
  public:
    // Initialize with a given size bound but do not construct the objects.
    explicit FixedArray(std::size_t limit) {
      Init(limit);
    }

    FixedArray() 
      : newed_end_(NULL) 
#ifndef NDEBUG
      , allocated_end_(NULL) 
#endif
    {}

    void Init(std::size_t count) {
      assert(!block_.get());
      block_.reset(malloc(sizeof(T) * count));
      if (!block_.get()) throw std::bad_alloc();
      newed_end_ = begin();
#ifndef NDEBUG
      allocated_end_ = begin() + count;
#endif
    }

    FixedArray(const FixedArray &from) {
      std::size_t size = from.newed_end_ - static_cast<const T*>(from.block_.get());
      Init(size);
      for (std::size_t i = 0; i < size; ++i) {
        push_back(from[i]);
      }
    }

    ~FixedArray() { clear(); }

    T *begin() { return static_cast<T*>(block_.get()); }
    const T *begin() const { return static_cast<const T*>(block_.get()); }
    // Always call Constructed after successful completion of new.  
    T *end() { return newed_end_; }
    const T *end() const { return newed_end_; }

    T &back() { return *(end() - 1); }
    const T &back() const { return *(end() - 1); }

    std::size_t size() const { return end() - begin(); }
    bool empty() const { return begin() == end(); }

    T &operator[](std::size_t i) { return begin()[i]; }
    const T &operator[](std::size_t i) const { return begin()[i]; }

    template <class C> void push_back(const C &c) {
      new (end()) T(c); // use "placement new" syntax to initalize T in an already-allocated memory location
      Constructed();
    }

    void clear() {
      for (T *i = begin(); i != end(); ++i)
        i->~T();
      newed_end_ = begin();
    }

  protected:
    void Constructed() {
      ++newed_end_;
#ifndef NDEBUG
      assert(newed_end_ <= allocated_end_);
#endif
    }

  private:
    util::scoped_malloc block_;

    T *newed_end_;

#ifndef NDEBUG
    T *allocated_end_;
#endif
};

} // namespace util

#endif // UTIL_FIXED_ARRAY_H

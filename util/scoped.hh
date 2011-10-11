#ifndef UTIL_SCOPED__
#define UTIL_SCOPED__

#include "util/exception.hh"

/* Other scoped objects in the style of scoped_ptr. */
#include <cstddef>
#include <cstdlib>

namespace util {

template <class T, class R, R (*Free)(T*)> class scoped_thing {
  public:
    explicit scoped_thing(T *c = static_cast<T*>(0)) : c_(c) {}

    ~scoped_thing() { if (c_) Free(c_); }

    void reset(T *c) {
      if (c_) Free(c_);
      c_ = c;
    }

    T &operator*() { return *c_; }
    const T&operator*() const { return *c_; }
    T &operator->() { return *c_; }
    const T&operator->() const { return *c_; }

    T *get() { return c_; }
    const T *get() const { return c_; }

  private:
    T *c_;

    scoped_thing(const scoped_thing &);
    scoped_thing &operator=(const scoped_thing &);
};

class scoped_malloc {
  public:
    scoped_malloc() : p_(NULL) {}

    scoped_malloc(void *p) : p_(p) {}

    ~scoped_malloc() { std::free(p_); }

    void reset(void *p = NULL) {
      scoped_malloc other(p_);
      p_ = p;
    }

    void call_realloc(std::size_t to) {
      void *ret;
      UTIL_THROW_IF(!(ret = std::realloc(p_, to)) && to, util::ErrnoException, "realloc to " << to << " bytes failed.");
      p_ = ret;
    }

    void *get() { return p_; }
    const void *get() const { return p_; }

  private:
    void *p_;

    scoped_malloc(const scoped_malloc &);
    scoped_malloc &operator=(const scoped_malloc &);
};

// Hat tip to boost.  
template <class T> class scoped_array {
  public:
    explicit scoped_array(T *content = NULL) : c_(content) {}

    ~scoped_array() { delete [] c_; }

    T *get() { return c_; }
    const T* get() const { return c_; }

    T &operator*() { return *c_; }
    const T&operator*() const { return *c_; }

    T &operator->() { return *c_; }
    const T&operator->() const { return *c_; }

    T &operator[](std::size_t idx) { return c_[idx]; }
    const T &operator[](std::size_t idx) const { return c_[idx]; }

    void reset(T *to = NULL) {
      scoped_array<T> other(c_);
      c_ = to;
    }

  private:
    T *c_;
};

} // namespace util

#endif // UTIL_SCOPED__

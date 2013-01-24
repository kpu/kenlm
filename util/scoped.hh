#ifndef UTIL_SCOPED__
#define UTIL_SCOPED__
/* Other scoped objects in the style of scoped_ptr. */

#include "util/exception.hh"
#include <cstddef>

namespace util {

class MallocException : public ErrnoException {
  public:
    explicit MallocException(std::size_t requested) throw();
    ~MallocException() throw();
};

void *MallocOrThrow(std::size_t requested);
void *CallocOrThrow(std::size_t requested);

class scoped_malloc {
  public:
    scoped_malloc() : p_(NULL) {}

    scoped_malloc(void *p) : p_(p) {}

    ~scoped_malloc();

    void reset(void *p = NULL) {
      scoped_malloc other(p_);
      p_ = p;
    }

    void call_realloc(std::size_t to);

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

    T &operator[](std::size_t idx) { return c_[idx]; }
    const T &operator[](std::size_t idx) const { return c_[idx]; }

    void reset(T *to = NULL) {
      scoped_array<T> other(c_);
      c_ = to;
    }

  private:
    T *c_;

    scoped_array(const scoped_array &);
    void operator=(const scoped_array &);
};

template <class T> class scoped_ptr {
  public:
    explicit scoped_ptr(T *content = NULL) : c_(content) {}

    ~scoped_ptr() { delete c_; }

    T *get() { return c_; }
    const T* get() const { return c_; }

    T &operator*() { return *c_; }
    const T&operator*() const { return *c_; }

    T *operator->() { return c_; }
    const T*operator->() const { return c_; }

    T &operator[](std::size_t idx) { return c_[idx]; }
    const T &operator[](std::size_t idx) const { return c_[idx]; }

    void reset(T *to = NULL) {
      scoped_ptr<T> other(c_);
      c_ = to;
    }

  private:
    T *c_;

    scoped_ptr(const scoped_ptr &);
    void operator=(const scoped_ptr &);
};

} // namespace util

#endif // UTIL_SCOPED__

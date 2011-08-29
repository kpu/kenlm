#ifndef UTIL_SCOPED__
#define UTIL_SCOPED__

/* Other scoped objects in the style of scoped_ptr. */

#include <cstddef>
#include <cstdio>
#include <stdlib.h>

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

template <class T> class scoped_malloc {
  public:
    scoped_malloc() : p_(NULL) {}

    scoped_malloc(T *p) : p_(p) {}

    ~scoped_malloc() { free(p_); }

    void reset(T *p) {
      scoped_malloc other(p_);
      p_ = p;
    }

    T &operator*() { return *p_; }
    const T &operator*() const { return *p_; }
    T &operator->() { return *p_; }
    const T&operator->() const { return *p_; }

    T *get() { return p_; }
    const T *get() const { return p_; }

  private:
    T *p_;

    scoped_malloc(const scoped_malloc &);
    scoped_malloc &operator=(const scoped_malloc &);
};

class scoped_fd {
  public:
    scoped_fd() : fd_(-1) {}

    explicit scoped_fd(int fd) : fd_(fd) {}

    ~scoped_fd();

    void reset(int to) {
      scoped_fd other(fd_);
      fd_ = to;
    }

    int get() const { return fd_; }

    int operator*() const { return fd_; }

    int release() {
      int ret = fd_;
      fd_ = -1;
      return ret;
    }

  private:
    int fd_;

    scoped_fd(const scoped_fd &);
    scoped_fd &operator=(const scoped_fd &);
};

class scoped_FILE {
  public:
    explicit scoped_FILE(std::FILE *file = NULL) : file_(file) {}

    ~scoped_FILE();

    std::FILE *get() { return file_; }
    const std::FILE *get() const { return file_; }

    void reset(std::FILE *to = NULL) {
      scoped_FILE other(file_);
      file_ = to;
    }

  private:
    std::FILE *file_;
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

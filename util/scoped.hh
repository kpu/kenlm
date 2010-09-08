#ifndef UTIL_SCOPED__
#define UTIL_SCOPED__

#include <boost/noncopyable.hpp>

#include <cstddef>

namespace util {

template <class T, class R, R (*Free)(T*)> class scoped_thing : boost::noncopyable {
  public:
    explicit scoped_thing(T *c = static_cast<T*>(0)) : c_(c) {}

    ~scoped_thing() { if (c_) Free(c_); }

    void reset(T *c) {
      if (c_) Free(c_);
      c_ = c;
    }

    T &operator*() { return *c_; }
    T &operator->() { return *c_; }

    T *get() { return c_; }
    const T *get() const { return c_; }

  private:
    T *c_;
};

class scoped_fd : boost::noncopyable {
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

  private:
    int fd_;
};

// (void*)-1 is MAP_FAILED; this is done to avoid including the mmap header here.  
class scoped_mmap : boost::noncopyable {
  public:
    scoped_mmap() : data_((void*)-1), size_(0) {}
    scoped_mmap(void *data, std::size_t size) : data_(data), size_(size) {}
    ~scoped_mmap();

    void *get() const { return data_; }

    const char *begin() const { return reinterpret_cast<char*>(data_); }
    const char *end() const { return reinterpret_cast<char*>(data_) + size_; }
    std::size_t size() const { return size_; }

    void reset(void *data, std::size_t size) {
      scoped_mmap other(data_, size_);
      data_ = data;
      size_ = size;
    }

    void reset() {
      reset((void*)-1, 0);
    }

  private:
    void *data_;
    std::size_t size_;
};

 
} // namespace util

#endif // UTIL_SCOPED__

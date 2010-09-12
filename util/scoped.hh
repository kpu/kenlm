#ifndef UTIL_SCOPED__
#define UTIL_SCOPED__

/* Other scoped objects in the style of scoped_ptr. */

#include <cstddef>

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
    T &operator->() { return *c_; }

    T *get() { return c_; }
    const T *get() const { return c_; }

  private:
    T *c_;

    scoped_thing(const scoped_thing &);
    scoped_thing &operator=(const scoped_thing &);
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

  private:
    int fd_;

    scoped_fd(const scoped_fd &);
    scoped_fd &operator=(const scoped_fd &);
};

// (void*)-1 is MAP_FAILED; this is done to avoid including the mmap header here.  
class scoped_mmap {
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

    scoped_mmap(const scoped_mmap &);
    scoped_mmap &operator=(const scoped_mmap &);
};

/* For when the memory might come from mmap or new char[] */
class scoped_mmap_or_array {
  private:
    typedef enum {MMAP_ALLOCATED, ARRAY_ALLOCATED, NONE} Alloc;

  public:
    scoped_mmap_or_array() : data_(NULL), size_(0), source_(NONE) {}

    ~scoped_mmap_or_array() { reset(); }

    void *get() const { return data_; }
    const char *begin() const { return reinterpret_cast<char*>(data_); }
    const char *end() const { return reinterpret_cast<char*>(data_) + size_; }
    std::size_t size() const { return size_; }

    void reset() { internal_reset(NULL, 0, NONE); }

    void reset_mmap(void *data, std::size_t size) { internal_reset(data, size, MMAP_ALLOCATED); }

    void reset_array(void *data, std::size_t size) { internal_reset(data, size, ARRAY_ALLOCATED); }

  private:
    void internal_reset(void *data, std::size_t size, Alloc from);

    void *data_;
    std::size_t size_;

    Alloc source_;

    scoped_mmap_or_array(const scoped_mmap_or_array &);
    scoped_mmap_or_array &operator=(const scoped_mmap_or_array &);
};
 
} // namespace util

#endif // UTIL_SCOPED__

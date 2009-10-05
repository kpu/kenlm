#ifndef UTIL_SCOPED_H__
#define UTIL_SCOPED_H__

#include <boost/noncopyable.hpp>

namespace util {

template <class T, class R, R (*Free)(T*)> class scoped_thing : boost::noncopyable {
  public:
    explicit scoped_thing(T *c = NULL) : c_(c) {}

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
 
} // namespace util

#endif // UTIL_SCOPED_H__

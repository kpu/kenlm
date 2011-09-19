#ifndef UTIL_FILE__
#define UTIL_FILE__

#include <cstdio>
#include <unistd.h>

namespace util {

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

    operator bool() { return fd_ != -1; }

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

int OpenReadOrThrow(const char *name);

int CreateOrThrow(const char *name);

// Return value for SizeFile when it can't size properly.  
const off_t kBadSize = -1;
off_t SizeFile(int fd);

void ReadOrThrow(int fd, void *to, std::size_t size);
void WriteOrThrow(int fd, const void *data_void, std::size_t size);

void RemoveOrThrow(const char *name);

} // namespace util

#endif // UTIL_FILE__

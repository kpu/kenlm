#ifndef UTIL_FILE__
#define UTIL_FILE__

#include <cstddef>
#include <cstdio>
#include <string>

#include <stdint.h>

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

    std::FILE *release() {
      std::FILE *ret = file_;
      file_ = NULL;
      return ret;
    }

  private:
    std::FILE *file_;
};

// Open for read only.  
int OpenReadOrThrow(const char *name);
// Create file if it doesn't exist, truncate if it does.  Opened for write.   
int CreateOrThrow(const char *name);

// Return value for SizeFile when it can't size properly.  
const uint64_t kBadSize = (uint64_t)-1;
uint64_t SizeFile(int fd);

void ResizeOrThrow(int fd, uint64_t to);

std::size_t PartialRead(int fd, void *to, std::size_t size);
void ReadOrThrow(int fd, void *to, std::size_t size);
std::size_t ReadOrEOF(int fd, void *to_void, std::size_t size);

void WriteOrThrow(int fd, const void *data_void, std::size_t size);
void WriteOrThrow(FILE *to, const void *data, std::size_t size);

void FSyncOrThrow(int fd);

// Seeking
void SeekOrThrow(int fd, uint64_t off);
void AdvanceOrThrow(int fd, int64_t off);
void SeekEnd(int fd);

std::FILE *FDOpenOrThrow(scoped_fd &file);
std::FILE *FDOpenReadOrThrow(scoped_fd &file);

class TempMaker {
  public:
    explicit TempMaker(const std::string &prefix);

    // These will already be unlinked for you.  
    int Make() const;
    std::FILE *MakeFile() const;

  private:
    std::string base_;
};

} // namespace util

#endif // UTIL_FILE__

#include "util/file.hh"

#include "util/exception.hh"

#include <cstdlib>
#include <cstdio>
#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

namespace util {

scoped_fd::~scoped_fd() {
  if (fd_ != -1 && close(fd_)) {
    std::cerr << "Could not close file " << fd_ << std::endl;
    std::abort();
  }
}

scoped_FILE::~scoped_FILE() {
  if (file_ && std::fclose(file_)) {
    std::cerr << "Could not close file " << std::endl;
    std::abort();
  }
}

int OpenReadOrThrow(const char *name) {
  int ret;
  UTIL_THROW_IF(-1 == (ret = open(name, O_RDONLY)), ErrnoException, "while opening " << name);
  return ret;
}

int CreateOrThrow(const char *name) {
  int ret;
  UTIL_THROW_IF(-1 == (ret = open(name, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR)), ErrnoException, "while creating " << name);
  return ret;
}

off_t SizeFile(int fd) {
  struct stat sb;
  if (fstat(fd, &sb) == -1 || (!sb.st_size && !S_ISREG(sb.st_mode))) return kBadSize;
  return sb.st_size;
}

void ReadOrThrow(int fd, void *to_void, std::size_t amount) {
  uint8_t *to = static_cast<uint8_t*>(to_void);
  while (amount) {
    ssize_t ret = read(fd, to, amount);
    if (ret == -1) UTIL_THROW(ErrnoException, "Reading " << amount << " from fd " << fd << " failed.");
    if (ret == 0) UTIL_THROW(Exception, "Hit EOF in fd " << fd << " but there should be " << amount << " more bytes to read.");
    amount -= ret;
    to += ret;
  }
}

void WriteOrThrow(int fd, const void *data_void, std::size_t size) {
  const uint8_t *data = static_cast<const uint8_t*>(data_void);
  while (size) {
    ssize_t ret = write(fd, data, size);
    if (ret < 1) UTIL_THROW(util::ErrnoException, "Write failed");
    data += ret;
    size -= ret;
  }
}

void RemoveOrThrow(const char *name) {
  UTIL_THROW_IF(std::remove(name), util::ErrnoException, "Could not remove " << name);
}

} // namespace util

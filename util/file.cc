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

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

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
#if defined(_WIN32) || defined(_WIN64)
  UTIL_THROW_IF(-1 == (ret = _open(name, _O_BINARY | _O_RDONLY)), ErrnoException, "while opening " << name);
#else
  UTIL_THROW_IF(-1 == (ret = open(name, O_RDONLY)), ErrnoException, "while opening " << name);
#endif
  return ret;
}

int CreateOrThrow(const char *name) {
  int ret;
#if defined(_WIN32) || defined(_WIN64)
  UTIL_THROW_IF(-1 == (ret = _open(name, _O_CREAT | _O_TRUNC | _O_RDWR, _S_IREAD | _S_IWRITE)), ErrnoException, "while creating " << name);
#else
  UTIL_THROW_IF(-1 == (ret = open(name, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)), ErrnoException, "while creating " << name);
#endif
  return ret;
}

uint64_t SizeFile(int fd) {
  struct stat sb;
  if (fstat(fd, &sb) == -1 || (!sb.st_size && !S_ISREG(sb.st_mode))) return kBadSize;
  return sb.st_size;
}

void ReadOrThrow(int fd, void *to_void, std::size_t amount) {
  uint8_t *to = static_cast<uint8_t*>(to_void);
  while (amount) {
    ssize_t ret = read(fd, to, amount);
    UTIL_THROW_IF(ret == -1, ErrnoException, "Reading " << amount << " from fd " << fd << " failed.");
    UTIL_THROW_IF(ret == 0, EndOfFileException, "Hit EOF in fd " << fd << " but there should be " << amount << " more bytes to read.");
    amount -= ret;
    to += ret;
  }
}

std::size_t ReadOrEOF(int fd, void *to_void, std::size_t amount) {
  uint8_t *to = static_cast<uint8_t*>(to_void);
  std::size_t remaining = amount;
  while (remaining) {
    ssize_t ret = read(fd, to, remaining);
    UTIL_THROW_IF(ret == -1, ErrnoException, "Reading " << remaining << " from fd " << fd << " failed.");
    if (!ret) return amount - remaining;
    remaining -= ret;
    to += ret;
  }
  return amount;
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

namespace {
void InternalSeek(int fd, off_t off, int whence) {
  UTIL_THROW_IF((off_t)-1 == lseek(fd, off, whence), ErrnoException, "Seek failed");
}
} // namespace

void SeekOrThrow(int fd, off_t off) {
  InternalSeek(fd, off, SEEK_SET);
}

void AdvanceOrThrow(int fd, off_t off) {
  InternalSeek(fd, off, SEEK_CUR);
}

void SeekEnd(int fd) {
  InternalSeek(fd, 0, SEEK_END);
}

} // namespace util

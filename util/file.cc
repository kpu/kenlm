#include "util/file.hh"

#include "util/exception.hh"

#include <cstdlib>
#include <cstdio>
#include <iostream>

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <io.h>
#include <algorithm>
#include <limits.h>
#else
#include <unistd.h>
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
  UTIL_THROW_IF(-1 == (ret = _open(name, _O_CREAT | _O_TRUNC | _O_RDWR | _O_BINARY, _S_IREAD | _S_IWRITE)), ErrnoException, "while creating " << name);
#else
  UTIL_THROW_IF(-1 == (ret = open(name, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)), ErrnoException, "while creating " << name);
#endif
  return ret;
}

uint64_t SizeFile(int fd) {
#if defined(_WIN32) || defined(_WIN64)
  __int64 ret = _filelengthi64(fd);
  return (ret == -1) ? kBadSize : ret;
#else
  struct stat sb;
  if (fstat(fd, &sb) == -1 || (!sb.st_size && !S_ISREG(sb.st_mode))) return kBadSize;
  return sb.st_size;
#endif
}

void ResizeOrThrow(int fd, uint64_t to) {
#if defined(_WIN32) || defined(_WIN64)
  UTIL_THROW_IF(_chsize_s(fd, to), ErrnoException, "Resizing to " << to << " bytes failed");
#else
  UTIL_THROW_IF(ftruncate(fd, to), ErrnoException, "Resizing to " << to << " bytes failed");
#endif
}

std::size_t PartialRead(int fd, void *to, std::size_t amount) {
#if defined(_WIN32) || defined(_WIN64)
  amount = min(static_cast<std::size_t>(INT_MAX), amount);
  int ret = _read(fd, to, amount); 
#else
  ssize_t ret = read(fd, to, amount);
#endif
  UTIL_THROW_IF(ret < 0, ErrnoException, "Reading " << amount << " from fd " << fd << " failed.");
  return static_cast<std::size_t>(ret);
}

void ReadOrThrow(int fd, void *to_void, std::size_t amount) {
  uint8_t *to = static_cast<uint8_t*>(to_void);
  while (amount) {
    std::size_t ret = PartialRead(fd, to, amount);
    UTIL_THROW_IF(ret == 0, EndOfFileException, "Hit EOF in fd " << fd << " but there should be " << amount << " more bytes to read.");
    amount -= ret;
    to += ret;
  }
}

std::size_t ReadOrEOF(int fd, void *to_void, std::size_t amount) {
  uint8_t *to = static_cast<uint8_t*>(to_void);
  std::size_t remaining = amount;
  while (remaining) {
    std::size_t ret = PartialRead(fd, to, remaining);
    if (!ret) return amount - remaining;
    remaining -= ret;
    to += ret;
  }
  return amount;
}

void WriteOrThrow(int fd, const void *data_void, std::size_t size) {
  const uint8_t *data = static_cast<const uint8_t*>(data_void);
  while (size) {
#if defined(_WIN32) || defined(_WIN64)
    int ret = write(fd, data, min(static_cast<std::size_t>(INT_MAX), size));
#else
    ssize_t ret = write(fd, data, size);
#endif
    if (ret < 1) UTIL_THROW(util::ErrnoException, "Write failed");
    data += ret;
    size -= ret;
  }
}

void WriteOrThrow(FILE *to, const void *data, std::size_t size) {
  assert(size);
  UTIL_THROW_IF(1 != std::fwrite(data, size, 1, to), util::ErrnoException, "Short write; requested size " << size);
}

void FSyncOrThrow(int fd) {
// Apparently windows doesn't have fsync?  
#if !defined(_WIN32) && !defined(_WIN64)
  UTIL_THROW_IF(-1 == fsync(fd), ErrnoException, "Sync of " << fd << " failed.");
#endif
}

namespace {
void InternalSeek(int fd, int64_t off, int whence) {
#if defined(_WIN32) || defined(_WIN64)
  UTIL_THROW_IF((__int64)-1 == _lseeki64(fd, off, whence), ErrnoException, "Windows seek failed");

#else
  UTIL_THROW_IF((off_t)-1 == lseek(fd, off, whence), ErrnoException, "Seek failed");
#endif
}
} // namespace

void SeekOrThrow(int fd, uint64_t off) {
  InternalSeek(fd, off, SEEK_SET);
}

void AdvanceOrThrow(int fd, int64_t off) {
  InternalSeek(fd, off, SEEK_CUR);
}

void SeekEnd(int fd) {
  InternalSeek(fd, 0, SEEK_END);
}

std::FILE *FDOpenOrThrow(scoped_fd &file) {
  std::FILE *ret = fdopen(file.get(), "r+b");
  if (!ret) UTIL_THROW(util::ErrnoException, "Could not fdopen descriptor " << file.get());
  file.release();
  return ret;
}

std::FILE *FDOpenReadOrThrow(scoped_fd &file) {
  std::FILE *ret = fdopen(file.get(), "rb");
  if (!ret) UTIL_THROW(util::ErrnoException, "Could not fdopen descriptor " << file.get());
  file.release();
  return ret;
}

TempMaker::TempMaker(const std::string &prefix) : base_(prefix) {
  base_ += "XXXXXX";
}

// Sigh.  Windows temporary file creation is full of race conditions.
#if defined(_WIN32) || defined(_WIN64)
/* mkstemp extracted from libc/sysdeps/posix/tempname.c.  Copyright
   (C) 1991-1999, 2000, 2001, 2006 Free Software Foundation, Inc.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.  */

/* This has been modified from the original version to rename the function and
 * set the Windows temporary flag. */

static const char letters[] =
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

/* Generate a temporary file name based on TMPL.  TMPL must match the
   rules for mk[s]temp (i.e. end in "XXXXXX").  The name constructed
   does not exist at the time of the call to mkstemp.  TMPL is
   overwritten with the result.  */
int
mkstemp_and_unlink(char *tmpl)
{
  int len;
  char *XXXXXX;
  static unsigned long long value;
  unsigned long long random_time_bits;
  unsigned int count;
  int fd = -1;
  int save_errno = errno;

  /* A lower bound on the number of temporary files to attempt to
     generate.  The maximum total number of temporary file names that
     can exist for a given template is 62**6.  It should never be
     necessary to try all these combinations.  Instead if a reasonable
     number of names is tried (we define reasonable as 62**3) fail to
     give the system administrator the chance to remove the problems.  */
#define ATTEMPTS_MIN (62 * 62 * 62)

  /* The number of times to attempt to generate a temporary file.  To
     conform to POSIX, this must be no smaller than TMP_MAX.  */
#if ATTEMPTS_MIN < TMP_MAX
  unsigned int attempts = TMP_MAX;
#else
  unsigned int attempts = ATTEMPTS_MIN;
#endif

  len = strlen (tmpl);
  if (len < 6 || strcmp (&tmpl[len - 6], "XXXXXX"))
    {
      errno = EINVAL;
      return -1;
    }

/* This is where the Xs start.  */
  XXXXXX = &tmpl[len - 6];

  /* Get some more or less random data.  */
  {
    SYSTEMTIME      stNow;
    FILETIME ftNow;

    // get system time
    GetSystemTime(&stNow);
    stNow.wMilliseconds = 500;
    if (!SystemTimeToFileTime(&stNow, &ftNow))
    {
        errno = -1;
        return -1;
    }

    random_time_bits = (((unsigned long long)ftNow.dwHighDateTime << 32)
                        | (unsigned long long)ftNow.dwLowDateTime);
  }
  value += random_time_bits ^ (unsigned long long)GetCurrentThreadId ();

  for (count = 0; count < attempts; value += 7777, ++count)
  {
    unsigned long long v = value;

    /* Fill in the random bits.  */
    XXXXXX[0] = letters[v % 62];
    v /= 62;
    XXXXXX[1] = letters[v % 62];
    v /= 62;
    XXXXXX[2] = letters[v % 62];
    v /= 62;
    XXXXXX[3] = letters[v % 62];
    v /= 62;
    XXXXXX[4] = letters[v % 62];
    v /= 62;
    XXXXXX[5] = letters[v % 62];

    /* Modified for windows and to unlink */
    //      fd = open (tmpl, O_RDWR | O_CREAT | O_EXCL, _S_IREAD | _S_IWRITE);
    int flags = _O_RDWR | _O_CREAT | _O_EXCL | _O_BINARY;
    flags |= _O_TEMPORARY;
    fd = _open (tmpl, flags, _S_IREAD | _S_IWRITE);
    if (fd >= 0)
    {
      errno = save_errno;
      return fd;
    }
    else if (errno != EEXIST)
      return -1;
  }

  /* We got out of the loop because we ran out of combinations to try.  */
  errno = EEXIST;
  return -1;
}
#else
int
mkstemp_and_unlink(char *tmpl) {
  int ret = mkstemp(tmpl);
  if (ret != -1) {
    UTIL_THROW_IF(unlink(tmpl), util::ErrnoException, "Failed to delete " << tmpl);
  }
  return ret;
}
#endif

int TempMaker::Make() const {
  std::string name(base_);
  name.push_back(0);
  int ret;
  UTIL_THROW_IF(-1 == (ret = mkstemp_and_unlink(&name[0])), util::ErrnoException, "Failed to make a temporary based on " << base_);
  return ret;
}

std::FILE *TempMaker::MakeFile() const {
  util::scoped_fd file(Make());
  return FDOpenOrThrow(file);
}

} // namespace util

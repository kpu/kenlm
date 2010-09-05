#include "util/scoped.hh"

#include <err.h>
#include <sys/mman.h>
#include <unistd.h>

namespace util {

scoped_fd::~scoped_fd() {
  if (fd_ != -1 && !close(fd_)) err(1, "Could not close file %i", fd_);
}

scoped_mmap::~scoped_mmap() {
  if (data_ != (void*)-1) {
    if (munmap(data_, size_))
      err(1, "Couldn't munmap language model memory");
  }
}

} // namespace util

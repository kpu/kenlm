#include "util/scoped.hh"

#include <err.h>
#include <sys/mman.h>
#include <unistd.h>

namespace util {

scoped_fd::~scoped_fd() {
  if (fd_ != -1 && close(fd_)) err(1, "Could not close file %i", fd_);
}

scoped_mmap::~scoped_mmap() {
  if (data_ != (void*)-1) {
    if (munmap(data_, size_))
      err(1, "Couldn't munmap language model memory");
  }
}

void scoped_mmap_or_array::internal_reset(void *data, std::size_t size, Alloc source) {
  switch(source_) {
    case MMAP_ALLOCATED:
      scoped_mmap(data_, size_);
      break;
    case ARRAY_ALLOCATED:
      delete [] reinterpret_cast<char*>(data_);
      break;
    case NONE:
      break;
  }
  data_ = data;
  size_ = size;
  source_ = source;
}

} // namespace util

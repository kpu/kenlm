#include "util/scoped.hh"

#include <assert.h>
#include <err.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
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

scoped_FILE::~scoped_FILE() {
  if (fclose(file_)) err(1, "Could not close file");
}

void scoped_memory::reset(void *data, std::size_t size, Alloc source) {
  switch(source_) {
    case MMAP_ALLOCATED:
      scoped_mmap(data_, size_);
      break;
    case ARRAY_ALLOCATED:
      delete [] reinterpret_cast<char*>(data_);
      break;
    case MALLOC_ALLOCATED:
      free(data_);
      break;
    case NONE_ALLOCATED:
      break;
  }
  data_ = data;
  size_ = size;
  source_ = source;
}

void scoped_memory::call_realloc(std::size_t size) {
  assert(source_ == MALLOC_ALLOCATED || source_ == NONE_ALLOCATED);
  void *new_data = realloc(data_, size);
  if (!new_data) {
    reset();
  } else {
    reset(new_data, size, MALLOC_ALLOCATED);
  }
}

} // namespace util

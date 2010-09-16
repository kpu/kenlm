#include "util/exception.hh"
#include "util/mmap.hh"
#include "util/scoped.hh"

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>

namespace util {

scoped_mmap::~scoped_mmap() {
  if (data_ != (void*)-1) {
    if (munmap(data_, size_))
      err(1, "munmap failed ");
  }
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

void *MapOrThrow(std::size_t size, bool for_write, int flags, bool prefault, int fd, off_t offset) {
#ifdef MAP_POPULATE // Linux specific
  if (prefault) {
    flags |= MAP_POPULATE;
  }
  int protect = for_write ? (PROT_READ | PROT_WRITE) : PROT_READ;
#endif
  void *ret = mmap(NULL, size, protect, flags, fd, offset);
  if (ret == MAP_FAILED) {
    UTIL_THROW(ErrnoException, "mmap failed for size " << size << " at offset " << offset);
  }
  return ret;
}

void *MapForRead(std::size_t size, bool prefault, int fd, off_t offset) {
  return MapOrThrow(size, false, MAP_FILE | MAP_PRIVATE, prefault, fd, offset);
}

void *MapAnonymous(std::size_t size) {
  return MapOrThrow(size, true,
#ifdef MAP_ANONYMOUS
      MAP_ANONYMOUS // Linux
#else
      MAP_ANON // BSD
#endif
      | MAP_PRIVATE, false, -1, 0);
}

void MapZeroedWrite(const char *name, std::size_t size, scoped_fd &file, scoped_mmap &mem) {
  file.reset(open(name, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));
  if (-1 == file.get())
    UTIL_THROW(ErrnoException, "Failed to open " << name << " for writing");
  if (-1 == ftruncate(file.get(), size))
    UTIL_THROW(ErrnoException, "ftruncate on " << name << " to " << size << " failed");
  try {
    mem.reset(MapOrThrow(size, true, MAP_FILE | MAP_SHARED, false, file.get(), 0), size);
  } catch (ErrnoException &e) {
    e << " in file " << name;
    throw;
  }
}

} // namespace util

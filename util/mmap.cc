#include "util/exception.hh"
#include "util/file.hh"
#include "util/mmap.hh"

#include <iostream>

#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>

namespace util {

scoped_mmap::~scoped_mmap() {
  if (data_ != (void*)-1) {
    // Thanks Denis Filimonov for pointing out NFS likes msync first.  
    if (msync(data_, size_, MS_SYNC) || munmap(data_, size_)) {
      std::cerr << "msync or mmap failed for " << size_ << " bytes." << std::endl;
      abort();
    }
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
#endif
  int protect = for_write ? (PROT_READ | PROT_WRITE) : PROT_READ;
  void *ret = mmap(NULL, size, protect, flags, fd, offset);
  if (ret == MAP_FAILED) {
    UTIL_THROW(ErrnoException, "mmap failed for size " << size << " at offset " << offset);
  }
  return ret;
}

const int kFileFlags =
#ifdef MAP_FILE
  MAP_FILE | MAP_SHARED
#else
  MAP_SHARED
#endif
  ;

void MapRead(LoadMethod method, int fd, off_t offset, std::size_t size, scoped_memory &out) {
  switch (method) {
    case LAZY:
      out.reset(MapOrThrow(size, false, kFileFlags, false, fd, offset), size, scoped_memory::MMAP_ALLOCATED);
      break;
    case POPULATE_OR_LAZY:
#ifdef MAP_POPULATE
    case POPULATE_OR_READ:
#endif
      out.reset(MapOrThrow(size, false, kFileFlags, true, fd, offset), size, scoped_memory::MMAP_ALLOCATED);
      break;
#ifndef MAP_POPULATE
    case POPULATE_OR_READ:
#endif
    case READ:
      out.reset(malloc(size), size, scoped_memory::MALLOC_ALLOCATED);
      if (!out.get()) UTIL_THROW(util::ErrnoException, "Allocating " << size << " bytes with malloc");
      if (-1 == lseek(fd, offset, SEEK_SET)) UTIL_THROW(ErrnoException, "lseek to " << offset << " in fd " << fd << " failed.");
      ReadOrThrow(fd, out.get(), size);
      break;
  }
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

void *MapZeroedWrite(const char *name, std::size_t size, scoped_fd &file) {
  file.reset(open(name, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));
  if (-1 == file.get())
    UTIL_THROW(ErrnoException, "Failed to open " << name << " for writing");
  if (-1 == ftruncate(file.get(), size))
    UTIL_THROW(ErrnoException, "ftruncate on " << name << " to " << size << " failed");
  try {
    return MapOrThrow(size, true, kFileFlags, false, file.get(), 0);
  } catch (ErrnoException &e) {
    e << " in file " << name;
    throw;
  }
}

} // namespace util

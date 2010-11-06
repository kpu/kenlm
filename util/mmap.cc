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
#endif
  int protect = for_write ? (PROT_READ | PROT_WRITE) : PROT_READ;
  void *ret = mmap(NULL, size, protect, flags, fd, offset);
  if (ret == MAP_FAILED) {
    UTIL_THROW(ErrnoException, "mmap failed for size " << size << " at offset " << offset);
  }
  return ret;
}

namespace {
void ReadAll(int fd, void *to_void, std::size_t amount) {
  uint8_t *to = static_cast<uint8_t*>(to_void);
  while (amount) {
    ssize_t ret = read(fd, to, amount);
    if (ret == -1) UTIL_THROW(ErrnoException, "Reading " << amount << " from fd " << fd << " failed.");
    if (ret == 0) UTIL_THROW(Exception, "Hit EOF in fd " << fd << " but there should be " << amount << " more bytes to read.");
    amount -= ret;
    to += ret;
  }
}
} // namespace

void MapRead(LoadMethod method, int fd, off_t offset, std::size_t size, scoped_memory &out) {
  switch (method) {
    case LAZY:
      out.reset(MapOrThrow(size, false, MAP_FILE | MAP_SHARED, false, fd, offset), size, scoped_memory::MMAP_ALLOCATED);
      break;
    case POPULATE_OR_LAZY:
#ifdef MAP_POPULATE
    case POPULATE_OR_READ:
#endif
      out.reset(MapOrThrow(size, false, MAP_FILE | MAP_SHARED, true, fd, offset), size, scoped_memory::MMAP_ALLOCATED);
      break;
#ifndef MAP_POPULATE
    case POPULATE_OR_READ:
#endif
    case READ:
      out.reset(malloc(size), size, scoped_memory::MALLOC_ALLOCATED);
      if (!out.get()) UTIL_THROW(util::ErrnoException, "Allocating " << size << " bytes with malloc");
      if (-1 == lseek(fd, offset, SEEK_SET)) UTIL_THROW(ErrnoException, "lseek to " << offset << " in fd " << fd << " failed.");
      ReadAll(fd, out.get(), size);
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
    return MapOrThrow(size, true, MAP_FILE | MAP_SHARED, false, file.get(), 0);
  } catch (ErrnoException &e) {
    e << " in file " << name;
    throw;
  }
}

} // namespace util

/* Memory mapping wrappers.
 * ARM and MinGW ports contributed by Hideo Okuma and Tomoyuki Yoshimura at
 * NICT.
 */
#include "util/mmap.hh"

#include "util/exception.hh"
#include "util/file.hh"
#include "util/parallel_read.hh"
#include "util/scoped.hh"

#include <iostream>

#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <io.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace util {

long SizePage() {
#if defined(_WIN32) || defined(_WIN64)
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return si.dwAllocationGranularity;
#else
  return sysconf(_SC_PAGE_SIZE);
#endif
}

void SyncOrThrow(void *start, size_t length) {
#if defined(_WIN32) || defined(_WIN64)
  UTIL_THROW_IF(!::FlushViewOfFile(start, length), ErrnoException, "Failed to sync mmap");
#else
  UTIL_THROW_IF(length && msync(start, length, MS_SYNC), ErrnoException, "Failed to sync mmap");
#endif
}

void UnmapOrThrow(void *start, size_t length) {
#if defined(_WIN32) || defined(_WIN64)
  UTIL_THROW_IF(!::UnmapViewOfFile(start), ErrnoException, "Failed to unmap a file");
#else
  UTIL_THROW_IF(munmap(start, length), ErrnoException, "munmap failed");
#endif
}

scoped_mmap::~scoped_mmap() {
  if (data_ != (void*)-1) {
    try {
      // Thanks Denis Filimonov for pointing out NFS likes msync first.  
      SyncOrThrow(data_, size_);
      UnmapOrThrow(data_, size_);
    } catch (const util::ErrnoException &e) {
      std::cerr << e.what();
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
    data_ = new_data;
    size_ = size;
    source_ = MALLOC_ALLOCATED;
  }
}

void *MapOrThrow(std::size_t size, bool for_write, int flags, bool prefault, int fd, uint64_t offset) {
#ifdef MAP_POPULATE // Linux specific
  if (prefault) {
    flags |= MAP_POPULATE;
  }
#endif
#if defined(_WIN32) || defined(_WIN64)
  int protectC = for_write ? PAGE_READWRITE : PAGE_READONLY;
  int protectM = for_write ? FILE_MAP_WRITE : FILE_MAP_READ;
  uint64_t total_size = size + offset;
  HANDLE hMapping = CreateFileMapping((HANDLE)_get_osfhandle(fd), NULL, protectC, total_size >> 32, static_cast<DWORD>(total_size), NULL);
  UTIL_THROW_IF(!hMapping, ErrnoException, "CreateFileMapping failed");
  LPVOID ret = MapViewOfFile(hMapping, protectM, offset >> 32, offset, size);
  CloseHandle(hMapping);
  UTIL_THROW_IF(!ret, ErrnoException, "MapViewOfFile failed");
#else
  int protect = for_write ? (PROT_READ | PROT_WRITE) : PROT_READ;
  void *ret;
  UTIL_THROW_IF((ret = mmap(NULL, size, protect, flags, fd, offset)) == MAP_FAILED, ErrnoException, "mmap failed for size " << size << " at offset " << offset);
#  ifdef MADV_HUGEPAGE
  /* We like huge pages but it's fine if we can't have them.  Note that huge
   * pages are not supported for file-backed mmap on linux.
   */
  madvise(ret, size, MADV_HUGEPAGE);
#  endif
#endif
  return ret;
}

const int kFileFlags =
#if defined(_WIN32) || defined(_WIN64)
  0 // MapOrThrow ignores flags on windows
#elif defined(MAP_FILE)
  MAP_FILE | MAP_SHARED
#else
  MAP_SHARED
#endif
  ;

void MapRead(LoadMethod method, int fd, uint64_t offset, std::size_t size, scoped_memory &out) {
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
      out.reset(MallocOrThrow(size), size, scoped_memory::MALLOC_ALLOCATED);
      SeekOrThrow(fd, offset);
      ReadOrThrow(fd, out.get(), size);
      break;
    case PARALLEL_READ:
      out.reset(MallocOrThrow(size), size, scoped_memory::MALLOC_ALLOCATED);
      ParallelRead(fd, out.get(), size, offset);
      break;
  }
}

// Allocates zeroed memory in to.
void MapAnonymous(std::size_t size, util::scoped_memory &to) {
  to.reset();
#if defined(_WIN32) || defined(_WIN64)
  to.reset(calloc(1, size), size, scoped_memory::MALLOC_ALLOCATED);
#else
  to.reset(MapOrThrow(size, true,
#  if defined(MAP_ANONYMOUS)
      MAP_ANONYMOUS | MAP_PRIVATE // Linux
#  else
      MAP_ANON | MAP_PRIVATE // BSD
#  endif
      , false, -1, 0), size, scoped_memory::MMAP_ALLOCATED);
#endif
}

void *MapZeroedWrite(int fd, std::size_t size) {
  ResizeOrThrow(fd, 0);
  ResizeOrThrow(fd, size);
  return MapOrThrow(size, true, kFileFlags, false, fd, 0);
}

void *MapZeroedWrite(const char *name, std::size_t size, scoped_fd &file) {
  file.reset(CreateOrThrow(name));
  try {
    return MapZeroedWrite(file.get(), size);
  } catch (ErrnoException &e) {
    e << " in file " << name;
    throw;
  }
}

Rolling::Rolling(const Rolling &copy_from, uint64_t increase) {
  *this = copy_from;
  IncreaseBase(increase);
}

Rolling &Rolling::operator=(const Rolling &copy_from) {
  fd_ = copy_from.fd_;
  file_begin_ = copy_from.file_begin_;
  file_end_ = copy_from.file_end_;
  for_write_ = copy_from.for_write_;
  block_ = copy_from.block_;
  read_bound_ = copy_from.read_bound_;

  current_begin_ = 0;
  if (copy_from.IsPassthrough()) {
    current_end_ = copy_from.current_end_;
    ptr_ = copy_from.ptr_;
  } else {
    // Force call on next mmap.
    current_end_ = 0;
    ptr_ = NULL;
  }
  return *this;
}

Rolling::Rolling(int fd, bool for_write, std::size_t block, std::size_t read_bound, uint64_t offset, uint64_t amount) {
  current_begin_ = 0;
  current_end_ = 0;
  fd_ = fd;
  file_begin_ = offset;
  file_end_ = offset + amount;
  for_write_ = for_write;
  block_ = block;
  read_bound_ = read_bound;
}

void *Rolling::ExtractNonRolling(scoped_memory &out, uint64_t index, std::size_t size) {
  out.reset();
  if (IsPassthrough()) return static_cast<uint8_t*>(get()) + index;
  uint64_t offset = index + file_begin_;
  // Round down to multiple of page size.
  uint64_t cruft = offset % static_cast<uint64_t>(SizePage());
  std::size_t map_size = static_cast<std::size_t>(size + cruft);
  out.reset(MapOrThrow(map_size, for_write_, kFileFlags, true, fd_, offset - cruft), map_size, scoped_memory::MMAP_ALLOCATED);
  return static_cast<uint8_t*>(out.get()) + static_cast<std::size_t>(cruft);
}

void Rolling::Roll(uint64_t index) {
  assert(!IsPassthrough());
  std::size_t amount;
  if (file_end_ - (index + file_begin_) > static_cast<uint64_t>(block_)) {
    amount = block_;
    current_end_ = index + amount - read_bound_;
  } else {
    amount = file_end_ - (index + file_begin_);
    current_end_ = index + amount;
  }
  ptr_ = static_cast<uint8_t*>(ExtractNonRolling(mem_, index, amount)) - index;

  current_begin_ = index;
}

} // namespace util

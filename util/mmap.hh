#ifndef UTIL_MMAP_H
#define UTIL_MMAP_H
// Utilities for mmaped files.  

#include <cstddef>
#include <limits>

#include <stdint.h>
#include <sys/types.h>

namespace util {

class scoped_fd;

long SizePage();

// (void*)-1 is MAP_FAILED; this is done to avoid including the mmap header here.  
class scoped_mmap {
  public:
    scoped_mmap() : data_((void*)-1), size_(0) {}
    scoped_mmap(void *data, std::size_t size) : data_(data), size_(size) {}
    ~scoped_mmap();

    void *get() const { return data_; }

    const uint8_t *begin() const { return reinterpret_cast<uint8_t*>(data_); }
    const uint8_t *end() const { return reinterpret_cast<uint8_t*>(data_) + size_; }
    std::size_t size() const { return size_; }

    void reset(void *data, std::size_t size) {
      scoped_mmap other(data_, size_);
      data_ = data;
      size_ = size;
    }

    void reset() {
      reset((void*)-1, 0);
    }

  private:
    void *data_;
    std::size_t size_;

    scoped_mmap(const scoped_mmap &);
    scoped_mmap &operator=(const scoped_mmap &);
};

/* For when the memory might come from mmap, new char[], or malloc.  Uses NULL
 * and 0 for blanks even though mmap signals errors with (void*)-1).  The reset
 * function checks that blank for mmap.  
 */
class scoped_memory {
  public:
    typedef enum {MMAP_ALLOCATED, ARRAY_ALLOCATED, MALLOC_ALLOCATED, NONE_ALLOCATED} Alloc;

    scoped_memory(void *data, std::size_t size, Alloc source) 
      : data_(data), size_(size), source_(source) {}

    scoped_memory() : data_(NULL), size_(0), source_(NONE_ALLOCATED) {}

    ~scoped_memory() { reset(); }

    void *get() const { return data_; }
    const char *begin() const { return reinterpret_cast<char*>(data_); }
    const char *end() const { return reinterpret_cast<char*>(data_) + size_; }
    std::size_t size() const { return size_; }

    Alloc source() const { return source_; }

    void reset() { reset(NULL, 0, NONE_ALLOCATED); }

    void reset(void *data, std::size_t size, Alloc from);

    // realloc allows the current data to escape hence the need for this call
    // If realloc fails, destroys the original too and get() returns NULL.
    void call_realloc(std::size_t to);

  private:
    void *data_;
    std::size_t size_;

    Alloc source_;

    scoped_memory(const scoped_memory &);
    scoped_memory &operator=(const scoped_memory &);
};

typedef enum {
  // mmap with no prepopulate
  LAZY,
  // On linux, pass MAP_POPULATE to mmap.
  POPULATE_OR_LAZY,
  // Populate on Linux.  malloc and read on non-Linux.  
  POPULATE_OR_READ,
  // malloc and read.  
  READ,
  // malloc and read in parallel (recommended for Lustre)
  PARALLEL_READ,
} LoadMethod;

extern const int kFileFlags;

// Wrapper around mmap to check it worked and hide some platform macros.  
void *MapOrThrow(std::size_t size, bool for_write, int flags, bool prefault, int fd, uint64_t offset = 0);

void MapRead(LoadMethod method, int fd, uint64_t offset, std::size_t size, scoped_memory &out);

void MapAnonymous(std::size_t size, scoped_memory &to);

// Open file name with mmap of size bytes, all of which are initially zero.  
void *MapZeroedWrite(int fd, std::size_t size);
void *MapZeroedWrite(const char *name, std::size_t size, scoped_fd &file);

// msync wrapper 
void SyncOrThrow(void *start, size_t length);

// Forward rolling memory map with no overlap.
class Rolling {
  public:
    Rolling() {}
    
    explicit Rolling(void *data) { Init(data); }

    Rolling(const Rolling &copy_from, uint64_t increase = 0);
    Rolling &operator=(const Rolling &copy_from);

    // For an actual rolling mmap.
    explicit Rolling(int fd, bool for_write, std::size_t block, std::size_t read_bound, uint64_t offset, uint64_t amount);

    // For a static mapping
    void Init(void *data) {
      ptr_ = data;
      current_end_ = std::numeric_limits<uint64_t>::max();
      current_begin_ = 0;
      // Mark as a pass-through.
      fd_ = -1;
    }

    void IncreaseBase(uint64_t by) {
      file_begin_ += by;
      ptr_ = static_cast<uint8_t*>(ptr_) + by;
      if (!IsPassthrough()) current_end_ = 0;
    }

    void DecreaseBase(uint64_t by) {
      file_begin_ -= by;
      ptr_ = static_cast<uint8_t*>(ptr_) - by;
      if (!IsPassthrough()) current_end_ = 0;
    }

    void *ExtractNonRolling(scoped_memory &out, uint64_t index, std::size_t size);

    // Returns base pointer
    void *get() const { return ptr_; }

    // Returns base pointer.
    void *CheckedBase(uint64_t index) {
      if (index >= current_end_ || index < current_begin_) {
        Roll(index);
      }
      return ptr_;
    }
    
    // Returns indexed pointer.
    void *CheckedIndex(uint64_t index) {
      return static_cast<uint8_t*>(CheckedBase(index)) + index;
    }

  private:
    void Roll(uint64_t index);

    // True if this is just a thin wrapper on a pointer.
    bool IsPassthrough() const { return fd_ == -1; }

    void *ptr_;
    uint64_t current_begin_;
    uint64_t current_end_;
    
    scoped_memory mem_;

    int fd_;
    uint64_t file_begin_;
    uint64_t file_end_;

    bool for_write_;
    std::size_t block_;
    std::size_t read_bound_;
};

} // namespace util

#endif // UTIL_MMAP_H

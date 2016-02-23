#ifndef UTIL_POOL_H
#define UTIL_POOL_H

#include <cstring>
#include <vector>

#include <stdint.h>

namespace util {

/* Very simple pool.  It can only allocate memory.  And all of the memory it
 * allocates must be freed at the same time.
 */
class Pool {
  public:
    Pool();

    ~Pool();

    void *Allocate(std::size_t size) {
      void *ret = current_;
      current_ += size;
      if (current_ <= current_end_) {
        return ret;
      } else {
        return More(size);
      }
    }

    // Continue the current allocation.  base must have been returned by the
    // MOST RECENT call to Allocate.
    void Continue(void *&base, std::size_t additional) {
      current_ += additional;
      if (current_ > current_end_) {
        std::size_t new_total = current_ - static_cast<uint8_t*>(base);
        void *new_base = More(new_total);
        std::memcpy(new_base, base, new_total - additional);
        base = new_base;
      }
    }

    void FreeAll();

  private:
    void *More(std::size_t size);

    std::vector<void *> free_list_;

    uint8_t *current_, *current_end_;

    // no copying
    Pool(const Pool &);
    Pool &operator=(const Pool &);
};

/**
 * Pool designed to allow limited freeing.
 * Keeps a linked list of free elements in the free spaces.
 * Will not reduce in size until FreeAll is called.
 */
class FreePool {
  public:
    explicit FreePool(std::size_t element_size)
      : free_list_(NULL), element_size_(element_size) {}

    void *Allocate() {
      if (free_list_) {
        void *ret = free_list_;
        free_list_ = *reinterpret_cast<void**>(free_list_);
        return ret;
      } else {
        return backing_.Allocate(element_size_);
      }
    }

    void Free(void *ptr) {
      *reinterpret_cast<void**>(ptr) = free_list_;
      free_list_ = ptr;
    }

    std::size_t ElementSize() const { return element_size_; }

  private:
    void *free_list_;

    Pool backing_;

    const std::size_t element_size_;
};

} // namespace util

#endif // UTIL_POOL_H

// Very simple pool.  It can only allocate memory.  And all of the memory it
// allocates must be freed at the same time.  

#ifndef UTIL_POOL_H
#define UTIL_POOL_H

#include <vector>

#include <stdint.h>
#include <string.h>

namespace util {

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
        memcpy(new_base, base, new_total - additional);
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

} // namespace util

#endif // UTIL_POOL_H

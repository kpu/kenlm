// Very simple pool.  It can only allocate memory.  And all of the memory it
// allocates must be freed at the same time.

#ifndef UTIL_POOL_H
#define UTIL_POOL_H

#include <vector>
#include <stdint.h>

namespace util {

class Pool {
  public:
    Pool();

    ~Pool();

    void *Allocate(std::size_t size) {
      void *ret = current_;
      current_ += size;
      if (current_ < current_end_) {
        return ret;
      } else {
        return More(size);
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

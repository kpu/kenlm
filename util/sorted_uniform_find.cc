#include "util/sorted_uniform_find.hh"

#include <inttypes.h>
#include <sys/types.h>

namespace util {

const uint64_t *SortedUniformFind(const uint64_t *left, const uint64_t *right, const uint64_t key) {
  typedef unsigned int local_uint128_t __attribute__((mode(TI)));
  if (right < left) return 0;
  while (1) {
    // These subtracts and compares should map to one subtraction on x86/amd64
    uint64_t off = key - *left;
    if (key <= *left) return (key == *left) ? left : 0;
    if (key >= *right) return (key == *right) ? right : 0;
		// now *left < key < *right so there is no division by 0 here and further left < right.
    const uint64_t *pivot = left + static_cast<size_t>(((local_uint128_t)off * (right - left)) / (*right - *left));
		// left <= pivot < right.
    if (*pivot > key) {
      right = pivot - 1;
    } else if (*pivot < key) {
      left = pivot + 1;
    } else {
      return pivot;
    }
		// Now left <= right.  Combined with the fact that left increases and right
		// decreases, *left and *right is ok.
  }
}

} // namespace util

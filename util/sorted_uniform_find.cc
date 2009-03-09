#include "util/sorted_uniform_find.hh"

#include <inttypes.h>
#include <sys/types.h>

namespace util {
namespace {
inline const uint64_t *Pivot(const uint64_t *left, const uint64_t *right, const uint64_t off) {
  typedef unsigned int local_uint128_t __attribute__((mode(TI)));
  return left + static_cast<size_t>(((local_uint128_t)off * (right - left)) / (*right - *left));
}

inline const uint32_t *Pivot(const uint32_t *left, const uint32_t *right, const uint32_t off) {
  return left + static_cast<size_t>(((uint64_t)off * (right - left)) / (*right - *left));
}

inline const uint16_t *Pivot(const uint16_t *left, const uint16_t *right, const uint16_t off) {
  return left + static_cast<size_t>(((size_t)off * (right - left)) / (*right - *left));
}
inline const uint8_t *Pivot(const uint8_t *left, const uint8_t *right, const uint8_t off) {
  return left + static_cast<size_t>(((size_t)off * (right - left)) / (*right - *left));
}

} // namespace

template <class Value> const Value *SortedUniformFind(const Value *left, const Value *right, const Value key) {
  if (right < left) return 0;
  while (1) {
    // These subtracts and compares should map to one subtraction on x86/amd64
    Value off = key - *left;
    if (key <= *left) return (key == *left) ? left : 0;
    if (key >= *right) return (key == *right) ? right : 0;
    // now *left < key < *right so there is no division by 0 here and further left < right.
    const Value *pivot = Pivot(left, right, off);
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

template const uint64_t *SortedUniformFind(const uint64_t *left, const uint64_t *right, const uint64_t key);
template const uint32_t *SortedUniformFind(const uint32_t *left, const uint32_t *right, const uint32_t key);
template const uint16_t *SortedUniformFind(const uint16_t *left, const uint16_t *right, const uint16_t key);
template const uint8_t *SortedUniformFind(const uint8_t *left, const uint8_t *right, const uint8_t key);

} // namespace util

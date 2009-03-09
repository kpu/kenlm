#include "util/sorted_uniform_find.hh"

#include <inttypes.h>
#include <sys/types.h>

namespace util {
namespace {

// In timing tests on Core 2 Duo and Opteron, float was faster than gcc's 128-bit support.
inline size_t Pivot(uint64_t off, uint64_t range, size_t width) {
  size_t ret = static_cast<size_t>(static_cast<float>(off) / static_cast<float>(range) * static_cast<float>(width));
	// Cap for floating point rounding
	return (ret <= width) ? ret : width;
}
inline size_t Pivot(uint32_t off, uint32_t range, size_t width) {
	return static_cast<size_t>(static_cast<uint64_t>(off) * static_cast<uint64_t>(width) / static_cast<uint64_t>(range));
}
inline size_t Pivot(uint16_t off, uint16_t range, size_t width) {
	return static_cast<size_t>(static_cast<size_t>(off) * width / static_cast<size_t>(range));
}
inline size_t Pivot(uint8_t off, uint8_t range, size_t width) {
	return static_cast<size_t>(static_cast<size_t>(off) * width / static_cast<size_t>(range));
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
    const Value *pivot = left + Pivot(off, *right - *left, right - left);
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

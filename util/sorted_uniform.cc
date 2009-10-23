#include "util/sorted_uniform.hh"

#include <inttypes.h>
#include <sys/types.h>

#include <stdlib.h>

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

template <class Value> const Value *SortedUniformBound(const Value *begin, const Value *end, const Value key) {
	while (begin != end) {
		if (key <= *begin) return begin;
		if (key > *(end - 1)) return end;
		if (key == *(end - 1)) return end - 1;
		Value off = key - *begin;
    const Value *pivot = begin + Pivot(off, *(end - 1) - *begin, end - begin);
		if (*pivot > key) {
			end = pivot;
		} else if (*pivot < key) {
		  begin = pivot + 1;
		} else {
			return pivot;
		}
	}
	return begin;
}

template const uint64_t *SortedUniformBound(const uint64_t *left, const uint64_t *right, const uint64_t key);
template const uint32_t *SortedUniformBound(const uint32_t *left, const uint32_t *right, const uint32_t key);
template const uint16_t *SortedUniformBound(const uint16_t *left, const uint16_t *right, const uint16_t key);
template const uint8_t *SortedUniformBound(const uint8_t *left, const uint8_t *right, const uint8_t key);

} // namespace util

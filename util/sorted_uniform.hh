#ifndef UTIL_SORTED_UNIFORM_H__
#define UTIL_SORTED_UNIFORM_H__

#include <inttypes.h>

namespace util {

/* This is a template, but it's not instantiated here because it depends on a
 * special-case Pivot function to do double-width arithmetic.
 * Currently uint64_t, uint32_t, uint16_t, and uint8_t are supported.
 *
 * If the key is unique then this returns the same as std::lower_bound and std::upper_bound.
 * Otherwise, this returns something in [std::lower_bound(begin, end, key), std::upper_bound(begin, end, key)]
 */
template <class Value> const Value *SortedUniformBound(const Value *begin, const Value *end, const Value key);

} // namespace util

#endif // UTIL_SORTED_UNIFORM_H__

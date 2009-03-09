#ifndef UTIL_SORTED_UNIFORM_FIND_H__
#define UTIL_SORTED_UNIFORM_FIND_H__

#include <inttypes.h>

namespace util {

/* This is a template, but it's not instantiated here because it depends on a
 * special-case Pivot function to do double-width arithmetic.
 * Currently uint64_t, uint32_t, uint16_t, and uint8_t are supported.
 */
template <class Value> const Value *SortedUniformFind(const Value *left, const Value *right, const Value key);

} // namespace util

#endif // UTIL_SORTED_UNIFORM_FIND_H__

#ifndef UTIL_SORTED_UNIFORM_FIND_H__
#define UTIL_SORTED_UNIFORM_FIND_H__

#include <inttypes.h>

namespace util {

const uint64_t *SortedUniformFind(const uint64_t *left, const uint64_t *right, uint64_t key);

} // namespace util

#endif // UTIL_SORTED_UNIFORM_FIND_H__

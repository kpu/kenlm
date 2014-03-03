#ifndef UTIL_MURMUR_HASH_H
#define UTIL_MURMUR_HASH_H
#include <cstddef>
#include <stdint.h>

namespace util {

uint64_t MurmurHash64A(const void * key, std::size_t len, uint64_t seed = 0);
uint64_t MurmurHash64B(const void * key, std::size_t len, uint64_t seed = 0);
uint64_t MurmurHashNative(const void * key, std::size_t len, uint64_t seed = 0);

} // namespace util

#endif // UTIL_MURMUR_HASH_H

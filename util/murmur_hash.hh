#ifndef UTIL_MURMUR_HASH__
#define UTIL_MURMUR_HASH__
#include <stdint.h>

uint64_t MurmurHash64A (const void * key, int len, unsigned int seed);
uint64_t MurmurHash64B (const void * key, int len, unsigned int seed);

#endif // UTIL_MURMUR_HASH__

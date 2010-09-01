#ifndef UTIL_MURMUR_HASH_H__
#define UTIL_MURMUR_HASH_H__
#include <stdint.h>

uint64_t MurmurHash64A (const void * key, int len, unsigned int seed);
uint64_t MurmurHash64B (const void * key, int len, unsigned int seed);

#endif // UTIL_MURMUR_HASH_H__

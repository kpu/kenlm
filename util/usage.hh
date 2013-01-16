#ifndef UTIL_USAGE__
#define UTIL_USAGE__
#include <cstddef>
#include <iosfwd>
#include <string>

#include <stdint.h>

namespace util {
void PrintUsage(std::ostream &to);

// Determine how much physical memory there is.  Return 0 on failure.
uint64_t GuessPhysicalMemory();

// Parse a size like unix sort.  Sadly, this means the default multiplier is K.
uint64_t ParseSize(const std::string &arg);
} // namespace util
#endif // UTIL_USAGE__

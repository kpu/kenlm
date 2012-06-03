#include "util/usage.hh"

#include <fstream>
#include <ostream>

#include <string.h>
#include <ctype.h>
#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/resource.h>
#include <sys/time.h>
#endif

namespace util {

namespace {
#if !defined(_WIN32) && !defined(_WIN64)
float FloatSec(const struct timeval &tv) {
  return static_cast<float>(tv.tv_sec) + (static_cast<float>(tv.tv_usec) / 1000000.0);
}
#endif
} // namespace

void PrintUsage(std::ostream &out) {
#if !defined(_WIN32) && !defined(_WIN64)
  struct rusage usage;
  if (getrusage(RUSAGE_SELF, &usage)) {
    perror("getrusage");
    return;
  }
  out << "user\t" << FloatSec(usage.ru_utime) << "\nsys\t" << FloatSec(usage.ru_stime) << '\n';

  // Linux doesn't set memory usage :-(.  
  std::ifstream status("/proc/self/status", std::ios::in);
  std::string line;
  while (getline(status, line)) {
    if (!strncmp(line.c_str(), "VmRSS:\t", 7)) {
      out << "VmRSS:  " << (line.c_str() + 7) << '\n';
      break;
    } else if (!strncmp(line.c_str(), "VmPeak:\t", 8)) {
      out << "VmPeak: " << (line.c_str() + 8) << '\n';
    }
  }
#endif
}

} // namespace util

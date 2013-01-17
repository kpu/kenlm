#include "util/usage.hh"

#include "util/exception.hh"

#include <fstream>
#include <ostream>
#include <sstream>

#include <string.h>
#include <ctype.h>
#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
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

uint64_t GuessPhysicalMemory() {
#if defined(_WIN32) || defined(_WIN64)
  return 0;
#elif defined(_SC_PHYS_PAGES) && defined(_SC_PAGESIZE)
  long pages = sysconf(_SC_PHYS_PAGES);
  if (pages == -1) return 0;
  long page_size = sysconf(_SC_PAGESIZE);
  if (page_size == -1) return 0;
  return static_cast<uint64_t>(pages) * static_cast<uint64_t>(page_size);
#else
  return 0;
#endif
}

namespace {
class SizeParseError : public Exception {
  public:
    explicit SizeParseError(const std::string &str) throw() {
      *this << "Failed to parse " << str << " into a memory size ";
    }
};

template <class Num> uint64_t ParseNum(const std::string &arg) {
  std::stringstream stream(arg);
  Num value;
  stream >> value;
  UTIL_THROW_IF_ARG(!stream, SizeParseError, (arg), "for the leading number.");
  std::string after;
  stream >> after;
  UTIL_THROW_IF_ARG(after.size() > 1, SizeParseError, (arg), "because there are more than two characters after the number.");
  std::string throwaway;
  UTIL_THROW_IF_ARG(stream >> throwaway, SizeParseError, (arg), "because there was more cruft " << throwaway << " after the number.");

  // Silly sort, using kilobytes as your default unit.  
  if (after.empty()) after == "K";
  if (after == "%") {
    uint64_t mem = GuessPhysicalMemory();
    UTIL_THROW_IF_ARG(!mem, SizeParseError, (arg), "because % was specified but the physical memory size could not be determined.");
    return static_cast<double>(value) * static_cast<double>(mem) / 100.0;
  }
  
  std::string units("bKMGTPEZY");
  std::string::size_type index = units.find(after[0]);
  UTIL_THROW_IF_ARG(index == std::string::npos, SizeParseError, (arg), "the allowed suffixes are " << units << "%.");
  for (std::string::size_type i = 0; i < index; ++i) {
    value *= 1024;
  }
  return value;
}

} // namespace

uint64_t ParseSize(const std::string &arg) {
  return arg.find('.') == std::string::npos ? ParseNum<double>(arg) : ParseNum<uint64_t>(arg);
}

} // namespace util

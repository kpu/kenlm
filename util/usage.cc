#include "util/usage.hh"

#include "util/exception.hh"

#include <fstream>
#include <ostream>
#include <sstream>
#include <set>
#include <string>

#include <string.h>
#include <ctype.h>
#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#endif

namespace util {

#if !defined(_WIN32) && !defined(_WIN64)
namespace {

// On Mac OS X, clock_gettime is not implemented.
// CLOCK_MONOTONIC is not defined either.
#ifdef __MACH__
#define CLOCK_MONOTONIC 0

int clock_gettime(int clk_id, struct timespec *tp) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  tp->tv_sec = tv.tv_sec;
  tp->tv_nsec = tv.tv_usec * 1000;
  return 0;
}
#endif // __MACH__

float FloatSec(const struct timeval &tv) {
  return static_cast<float>(tv.tv_sec) + (static_cast<float>(tv.tv_usec) / 1000000.0);
}
float FloatSec(const struct timespec &tv) {
  return static_cast<float>(tv.tv_sec) + (static_cast<float>(tv.tv_nsec) / 1000000000.0);
}

const char *SkipSpaces(const char *at) {
  for (; *at == ' ' || *at == '\t'; ++at) {}
  return at;
}

class RecordStart {
  public:
    RecordStart() {
      clock_gettime(CLOCK_MONOTONIC, &started_);
    }

    const struct timespec &Started() const {
      return started_;
    }

  private:
    struct timespec started_;
};

const RecordStart kRecordStart;
} // namespace
#endif

void PrintUsage(std::ostream &out) {
#if !defined(_WIN32) && !defined(_WIN64)
  // Linux doesn't set memory usage in getrusage :-(
  std::set<std::string> headers;
  headers.insert("VmPeak:");
  headers.insert("VmRSS:");
  headers.insert("Name:");

  std::ifstream status("/proc/self/status", std::ios::in);
  std::string header, value;
  while ((status >> header) && getline(status, value)) {
    if (headers.find(header) != headers.end()) {
      out << header << SkipSpaces(value.c_str()) << '\t';
    }
  }

  struct rusage usage;
  if (getrusage(RUSAGE_SELF, &usage)) {
    perror("getrusage");
    return;
  }
  out << "RSSMax:" << usage.ru_maxrss << " kB" << '\t';
  out << "user:" << FloatSec(usage.ru_utime) << "\tsys:" << FloatSec(usage.ru_stime) << '\t';
  out << "CPU:" << (FloatSec(usage.ru_utime) + FloatSec(usage.ru_stime));

  struct timespec current;
  clock_gettime(CLOCK_MONOTONIC, &current);
  out << "\treal:" << (FloatSec(current) - FloatSec(kRecordStart.Started())) << '\n';
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
  if (after.empty()) after = "K";
  if (after == "%") {
    uint64_t mem = GuessPhysicalMemory();
    UTIL_THROW_IF_ARG(!mem, SizeParseError, (arg), "because % was specified but the physical memory size could not be determined.");
    return static_cast<uint64_t>(static_cast<double>(value) * static_cast<double>(mem) / 100.0);
  }
  
  std::string units("bKMGTPEZY");
  std::string::size_type index = units.find(after[0]);
  UTIL_THROW_IF_ARG(index == std::string::npos, SizeParseError, (arg), "the allowed suffixes are " << units << "%.");
  for (std::string::size_type i = 0; i < index; ++i) {
    value *= 1024;
  }
  return static_cast<uint64_t>(value);
}

} // namespace

uint64_t ParseSize(const std::string &arg) {
  return arg.find('.') == std::string::npos ? ParseNum<double>(arg) : ParseNum<uint64_t>(arg);
}

} // namespace util

#include "util/scoped.hh"

#include <cstdlib>
#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/mman.h>
#endif

namespace util {

MallocException::MallocException(std::size_t requested) throw() {
  *this << "for " << requested << " bytes ";
}

MallocException::~MallocException() throw() {}

namespace {
void *InspectAddr(void *addr, std::size_t requested, const char *func_name) {
  UTIL_THROW_IF_ARG(!addr && requested, MallocException, (requested), "in " << func_name);
  // These routines are often used for large chunks of memory where huge pages help.
#if MADV_HUGEPAGE
  madvise(addr, requested, MADV_HUGEPAGE);
#endif
  return addr;
}
} // namespace

void *MallocOrThrow(std::size_t requested) {
  return InspectAddr(std::malloc(requested), requested, "malloc");
}

void *CallocOrThrow(std::size_t requested) {
  return InspectAddr(std::calloc(1, requested), requested, "calloc");
}

scoped_malloc::~scoped_malloc() {
  std::free(p_);
}

void scoped_malloc::call_realloc(std::size_t requested) {
  p_ = InspectAddr(std::realloc(p_, requested), requested, "realloc");
}

} // namespace util

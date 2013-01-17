#include "util/scoped.hh"

#include <cstdlib>

namespace util {

MallocException::MallocException(std::size_t requested) throw() {
  *this << "for " << requested << " bytes ";
}

MallocException::~MallocException() throw() {}

void *MallocOrThrow(std::size_t requested) {
  void *ret;
  UTIL_THROW_IF_ARG(!(ret = std::malloc(requested)), MallocException, (requested), "in malloc");
  return ret;
}

scoped_malloc::~scoped_malloc() {
  std::free(p_);
}

void scoped_malloc::call_realloc(std::size_t to) {
  void *ret;
  UTIL_THROW_IF_ARG(!(ret = std::realloc(p_, to)) && to, MallocException, (to), "in realloc");
  p_ = ret;
}

} // namespace util

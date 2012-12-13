#include "lm/builder/print.hh"

#include "util/file.hh"
#include "util/mmap.hh"

#include <string.h>

namespace lm { namespace builder {

VocabReconstitute::VocabReconstitute(int fd) {
  uint64_t size = util::SizeFile(fd);
  UTIL_THROW_IF(util::kBadSize == size, util::ErrnoException, "Vocabulary file should be sizeable");
  util::MapRead(util::POPULATE_OR_READ, fd, 0, size, memory_);
  const char *const start = static_cast<const char*>(memory_.get());
  for (const char *i = start; i != start + size; i += strlen(i) + 1) {
    map_.push_back(i);
  }
}

}} // namespaces

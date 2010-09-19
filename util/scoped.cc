#include "util/scoped.hh"

#include <err.h>
#include <unistd.h>

namespace util {

scoped_fd::~scoped_fd() {
  if (fd_ != -1 && close(fd_)) err(1, "Could not close file %i", fd_);
}

scoped_FILE::~scoped_FILE() {
  if (file_ && fclose(file_)) err(1, "Could not close file.");
}

} // namespace util

#include "util/scoped.hh"

#include <iostream>

#include <stdlib.h>
#include <unistd.h>

namespace util {

scoped_fd::~scoped_fd() {
  if (fd_ != -1 && close(fd_)) {
    std::cerr << "Could not close file " << fd_ << std::endl;
    abort();
  }
}

scoped_FILE::~scoped_FILE() {
  if (file_ && fclose(file_)) {
    std::cerr << "Could not close file " << std::endl;
    abort();
  }
}

} // namespace util

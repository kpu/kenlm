#include <boost/lexical_cast.hpp>
#include <boost/scoped_array.hpp>

#include <iostream>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

const int kExitInput = 1;
const int kExitOutput = 2;
const int kExitOther = 3;
const int kExitHelp = 4;

size_t ReadOrDie(int from_fd, char *to, size_t amount) {
  ssize_t got = read(from_fd, to, amount);
  if (!got) errx(kExitInput, "End of file reached before end byte.");
  if (got < 0) err(kExitInput, "read");
  return static_cast<size_t>(got);
}

void Fallback(int from_fd, int to_fd, loff_t begin, loff_t end) {
  if (lseek(from_fd, begin, SEEK_SET) != begin) {
    err(kExitInput, "lseek failed: is stdin a file?");
  }
  const size_t kBufferSize = 16384;
  boost::scoped_array<char> buffer(new char[kBufferSize]);
  while (begin < end) {
    size_t got = ReadOrDie(from_fd, buffer.get(), std::min<size_t>(kBufferSize, end - begin));
    size_t written = 0;
    while (written < got) {
      ssize_t wrote = write(to_fd, buffer.get() + written, got - written);
      if (wrote <= 0) errx(kExitOutput, "write");
      written += wrote;
    }
    begin += got;
  }
}

void CopyRange(int from_fd, int to_fd, loff_t begin, loff_t end) {
  while (begin < end) {
    ssize_t ret = splice(from_fd, &begin, to_fd, NULL, end - begin, 0);
    if (ret < 0) {
      if (errno == EINVAL) {
        Fallback(from_fd, to_fd, begin, end);
        return;
      } else {
        err(kExitOther, "splice");
      }
    }
    if (!ret) {
      errx(kExitInput, "End of file reached before end byte.");
    }
  }
}

loff_t RoundUp(int from_fd, char delim, loff_t start) {
  // Begin of file is implicitly delimited.  
  if (!start) return start;
  if (lseek(from_fd, start, SEEK_SET) != start) {
    err(kExitInput, "lseek failed: is stdin a file?");
  }
  const size_t kBufferSize = 1024;
  boost::scoped_array<char> buffer(new char[kBufferSize]);
  while (1) {
    ssize_t got = read(from_fd, buffer.get(), kBufferSize);
    // EOF
    if (!got) return start;
    if (got < 0) err(kExitInput, "RoundUp: read");
    for (const char * i = buffer.get(); i < buffer.get() + got; ++i) {
      if (*i == delim) {
        return start + (i - buffer.get());
      }
    }
    start += got;
  }
}

void Help(const char *name) {
  std::cerr << name << " begin end [up]\nWill select bytes [begin, end) from stdin, which must be seekable." << std::endl;
  exit(kExitHelp);
}

int main(int argc, char *argv[]) {
  if (argc < 3 || argc > 4) Help(argv[0]);
  loff_t begin = boost::lexical_cast<loff_t>(argv[1]);
  loff_t end = boost::lexical_cast<loff_t>(argv[2]);
  if (argc == 4) {
    if (!strcmp(argv[3], "up")) {
      // Why this order?  So that the later seek to begin is a noop.  
      end = RoundUp(0, '\n', end);
      begin = RoundUp(0, '\n', begin);
    } else {
      Help(argv[0]);
    }
  }
  std::cerr << "begin = " << begin << " end = " << end << std::endl;
  CopyRange(0, 1, begin, end);
  return 0;
}

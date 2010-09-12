#include "util/file_piece.hh"

#include "util/exception.hh"

#include <string>

#include <cstdlib>
#include <ctype.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace util {

EndOfFileException::EndOfFileException() throw() {
  stream_ << "End of file.";
}
EndOfFileException::~EndOfFileException() throw() {}

ParseNumberException::ParseNumberException(StringPiece value) throw() {
  stream_ << "Could not parse \"" << value << "\" into a float.";
}

namespace {
int OpenOrThrow(const char *name) {
  int ret = open(name, O_RDONLY);
  if (ret == -1) UTIL_THROW(ErrnoException, "in open (" << name << ") for reading.");
  return ret;
}

off_t SizeOrThrow(int fd, const char *name) {
  struct stat sb;
  if (fstat(fd, &sb) == -1) UTIL_THROW(ErrnoException, "in stat " << name);
  return sb.st_size;
}
} // namespace

FilePiece::FilePiece(const char *name, off_t min_buffer) : 
  file_(OpenOrThrow(name)), total_size_(SizeOrThrow(file_.get(), name)), page_(sysconf(_SC_PAGE_SIZE)) {
  
  default_map_size_ = page_ * std::max<off_t>((min_buffer / page_ + 1), 2);
  position_ = NULL;
  position_end_ = NULL;
  mapped_offset_ = data_.begin() - position_end_;
  at_end_ = false;
  Shift();
}

float FilePiece::ReadFloat() throw(EndOfFileException, ParseNumberException) {
  SkipSpaces();
  while (last_space_ < position_) {
    if (at_end_) {
      std::string buffer(position_, position_end_);
      char *end;
      float ret = std::strtof(buffer.c_str(), &end);
      if (buffer.c_str() == end) throw ParseNumberException(buffer);
      position_ += end - buffer.c_str();
      return ret;
    }
    Shift();
  }
  char *end;
  float ret = std::strtof(position_, &end);
  if (end == position_) throw ParseNumberException(ReadDelimited());
  position_ = end;
  return ret;
}

void FilePiece::SkipSpaces() throw (EndOfFileException) {
  for (; ; ++position_) {
    if (position_ == position_end_) Shift();
    if (!isspace(*position_)) return;
  }
}

const char *FilePiece::FindDelimiterOrEOF() throw (EndOfFileException) {
  for (const char *i = position_; i <= last_space_; ++i) {
    if (isspace(*i)) return i;
  }
  while (!at_end_) {
    size_t skip = position_end_ - position_;
    Shift();
    for (const char *i = position_ + skip; i <= last_space_; ++i) {
      if (isspace(*i)) return i;
    }
  }
  return position_end_;
}

StringPiece FilePiece::ReadLine(char delim) throw (EndOfFileException) {
  const char *start = position_;
  do {
    for (const char *i = start; i < position_end_; ++i) {
      if (*i == delim) {
        StringPiece ret(position_, i - position_);
        position_ = i + 1;
        return ret;
      }
    }
    size_t skip = position_end_ - position_;
    Shift();
    start = position_ + skip;
  } while (!at_end_);
  StringPiece ret(position_, position_end_ - position_);
  position_ = position_end_;
  return position_;
}

void FilePiece::Shift() throw(EndOfFileException) {
  if (at_end_) throw EndOfFileException();
  off_t desired_begin = position_ - data_.begin() + mapped_offset_;
  off_t ignore = desired_begin % page_;
  // Duplicate request for Shift means give more data.  
  if (position_ == data_.begin() + ignore) {
    default_map_size_ *= 2;
  }
  mapped_offset_ = desired_begin - ignore;

  // The normal operation of this loop is to run once.  However, it may run
  // multiple times if we can't find an enter character.
  off_t mapped_size;
  if (default_map_size_ >= total_size_ - mapped_offset_) {
    at_end_ = true;
    mapped_size = total_size_ - mapped_offset_;
  } else {
    mapped_size = default_map_size_;
  }
  data_.reset();
  data_.reset(mmap(NULL, mapped_size, PROT_READ, MAP_PRIVATE, *file_, mapped_offset_), mapped_size);
  if (data_.get() == MAP_FAILED) UTIL_THROW(ErrnoException, "mmap language model file for reading")
  position_ = data_.begin() + ignore;
  position_end_ = data_.begin() + mapped_size;
  for (last_space_ = position_end_ - 1; last_space_ >= position_; --last_space_) {
    if (isspace(*last_space_))  break;
  }
}

} // namespace util

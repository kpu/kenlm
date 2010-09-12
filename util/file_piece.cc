#include "util/file_piece.hh"

#include "util/exception.hh"

#include <iostream>
#include <string>
#include <limits>

#include <assert.h>
#include <cstdlib>
#include <ctype.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace util {

EndOfFileException::EndOfFileException() throw() {
  stream_ << "End of file";
}
EndOfFileException::~EndOfFileException() throw() {}

ParseNumberException::ParseNumberException(StringPiece value) throw() {
  stream_ << "Could not parse \"" << value << "\" into a float";
}

namespace {
int OpenOrThrow(const char *name) {
  int ret = open(name, O_RDONLY);
  if (ret == -1) UTIL_THROW(ErrnoException, "in open (" << name << ") for reading");
  return ret;
}
const off_t kBadSize = std::numeric_limits<off_t>::max();

off_t SizeFile(int fd) {
  struct stat sb;
  if (fstat(fd, &sb) == -1 || (!sb.st_size && !S_ISREG(sb.st_mode))) return kBadSize;
  return sb.st_size;
}
} // namespace

FilePiece::FilePiece(const char *name, std::ostream *show_progress, off_t min_buffer) : 
  file_(OpenOrThrow(name)), total_size_(SizeFile(file_.get())), page_(sysconf(_SC_PAGE_SIZE)),
  progress_(total_size_ == kBadSize ? NULL : show_progress, std::string("Reading ") + name, total_size_) {
  if (total_size_ == kBadSize) {
    fallback_to_read_ = true;
    if (show_progress) 
      *show_progress << "File " << name << " isn't normal.  Using slower read() instead of mmap().  No progress bar." << std::endl;
  } else {
    fallback_to_read_ = false;
  }
  default_map_size_ = page_ * std::max<off_t>((min_buffer / page_ + 1), 2);
  position_ = NULL;
  position_end_ = NULL;
  mapped_offset_ = 0;
  at_end_ = false;
  Shift();
}

float FilePiece::ReadFloat() throw(EndOfFileException, ParseNumberException) {
  SkipSpaces();
  while (last_space_ < position_) {
    if (at_end_) {
      // Hallucinate a null off the end of the file.
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
  progress_.Set(desired_begin);

  if (!fallback_to_read_) MMapShift(desired_begin);
  // Notice an mmap failure might set the fallback.  
  if (fallback_to_read_) ReadShift(desired_begin);

  for (last_space_ = position_end_ - 1; last_space_ >= position_; --last_space_) {
    if (isspace(*last_space_))  break;
  }
}

void FilePiece::MMapShift(off_t desired_begin) throw() {
  // Use mmap.  
  off_t ignore = desired_begin % page_;
  // Duplicate request for Shift means give more data.  
  if (position_ == data_.begin() + ignore) {
    default_map_size_ *= 2;
  }
  // Local version so that in case of failure it doesn't overwrite the class variable.  
  off_t mapped_offset = desired_begin - ignore;

  off_t mapped_size;
  if (default_map_size_ >= static_cast<size_t>(total_size_ - mapped_offset)) {
    at_end_ = true;
    mapped_size = total_size_ - mapped_offset;
  } else {
    mapped_size = default_map_size_;
  }

  // Forcibly clear the existing mmap first.  
  data_.reset();
  data_.reset(mmap(NULL, mapped_size, PROT_READ, MAP_PRIVATE, *file_, mapped_offset), mapped_size, scoped_memory::MMAP_ALLOCATED);
  if (data_.get() == MAP_FAILED) {
    fallback_to_read_ = true;
    return;
  }
  mapped_offset_ = mapped_offset;
  position_ = data_.begin() + ignore;
  position_end_ = data_.begin() + mapped_size;
}

void FilePiece::ReadShift(off_t desired_begin) throw() {
  assert(fallback_to_read_);
  if (data_.source() != scoped_memory::MALLOC_ALLOCATED) {
    // First call.
    data_.reset();
    data_.reset(malloc(default_map_size_), default_map_size_, scoped_memory::MALLOC_ALLOCATED);
    if (!data_.get()) UTIL_THROW(ErrnoException, "malloc failed for " << default_map_size_);
    position_ = data_.begin();
    position_end_ = position_;
  } 
  
  // Bytes [data_.begin(), position_) have been consumed.  
  // Bytes [position_, position_end_) have been read into the buffer.  

  // Start at the beginning of the buffer if there's nothing useful in it.  
  if (position_ == position_end_) {
    mapped_offset_ += (position_end_ - data_.begin());
    position_ = data_.begin();
    position_end_ = position_;
  }

  std::size_t already_read = position_end_ - data_.begin();

  if (already_read == default_map_size_) {
    if (position_ == data_.begin()) {
      // Buffer too small.  
      std::size_t valid_length = position_end_ - position_;
      default_map_size_ *= 2;
      data_.call_realloc(default_map_size_);
      if (!data_.get()) UTIL_THROW(ErrnoException, "realloc failed for " << default_map_size_);
      position_ = data_.begin();
      position_end_ = position_ + valid_length;
    } else {
      size_t moving = position_end_ - position_;
      memmove(data_.get(), position_, moving);
      position_ = data_.begin();
      position_end_ = position_ + moving;
      already_read = moving;
    }
  }

  ssize_t read_return = read(file_.get(), static_cast<char*>(data_.get()) + already_read, default_map_size_ - already_read);
  if (read_return == -1) UTIL_THROW(ErrnoException, "read failed");
  if (read_return == 0) at_end_ = true;
  position_end_ += read_return;
}

} // namespace util

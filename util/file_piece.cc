#include "util/file_piece.hh"

#include "util/exception.hh"

#include <string>
#include <limits>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

namespace util {

EndOfFileException::EndOfFileException() throw() {
  *this << "End of file";
}
EndOfFileException::~EndOfFileException() throw() {}

ParseNumberException::ParseNumberException(StringPiece value) throw() {
  *this << "Could not parse \"" << value << "\" into a float";
}

GZException::GZException(void *file) {
#ifdef HAVE_ZLIB
  int num;
  *this << gzerror(file, &num) << " from zlib";
#endif // HAVE_ZLIB
}

int OpenReadOrThrow(const char *name) {
  int ret = open(name, O_RDONLY);
  if (ret == -1) UTIL_THROW(ErrnoException, "in open (" << name << ") for reading");
  return ret;
}

off_t SizeFile(int fd) {
  struct stat sb;
  if (fstat(fd, &sb) == -1 || (!sb.st_size && !S_ISREG(sb.st_mode))) return kBadSize;
  return sb.st_size;
}

FilePiece::FilePiece(const char *name, std::ostream *show_progress, off_t min_buffer) throw (GZException) : 
  file_(OpenReadOrThrow(name)), total_size_(SizeFile(file_.get())), page_(sysconf(_SC_PAGE_SIZE)),
  progress_(total_size_ == kBadSize ? NULL : show_progress, std::string("Reading ") + name, total_size_) {
  Initialize(name, show_progress, min_buffer);
}

FilePiece::FilePiece(int fd, const char *name, std::ostream *show_progress, off_t min_buffer) throw (GZException) : 
  file_(fd), total_size_(SizeFile(file_.get())), page_(sysconf(_SC_PAGE_SIZE)),
  progress_(total_size_ == kBadSize ? NULL : show_progress, std::string("Reading ") + name, total_size_) {
  Initialize(name, show_progress, min_buffer);
}

FilePiece::~FilePiece() {
#ifdef HAVE_ZLIB
  if (gz_file_) {
    // zlib took ownership
    file_.release();
    int ret;
    if (Z_OK != (ret = gzclose(gz_file_))) {
      errx(1, "could not close file %s using zlib", file_name_.c_str());
      abort();
    }
  }
#endif
}

void FilePiece::Initialize(const char *name, std::ostream *show_progress, off_t min_buffer) throw (GZException) {
#ifdef HAVE_ZLIB
  gz_file_ = NULL;
#endif
  file_name_ = name;

  default_map_size_ = page_ * std::max<off_t>((min_buffer / page_ + 1), 2);
  position_ = NULL;
  position_end_ = NULL;
  mapped_offset_ = 0;
  at_end_ = false;

  if (total_size_ == kBadSize) {
    // So the assertion passes.  
    fallback_to_read_ = false;
    if (show_progress) 
      *show_progress << "File " << name << " isn't normal.  Using slower read() instead of mmap().  No progress bar." << std::endl;
    TransitionToRead();
  } else {
    fallback_to_read_ = false;
  }
  Shift();
  // gzip detect.
  if ((position_end_ - position_) > 2 && *position_ == 0x1f && static_cast<unsigned char>(*(position_ + 1)) == 0x8b) {
#ifndef HAVE_ZLIB
    UTIL_THROW(GZException, "Looks like a gzip file but support was not compiled in.");
#endif
    if (!fallback_to_read_) {
      at_end_ = false;
      TransitionToRead();
    }
  }
}

float FilePiece::ReadFloat() throw(GZException, EndOfFileException, ParseNumberException) {
  SkipSpaces();
  while (last_space_ < position_) {
    if (at_end_) {
      // Hallucinate a null off the end of the file.
      std::string buffer(position_, position_end_);
      char *end;
      float ret = strtof(buffer.c_str(), &end);
      if (buffer.c_str() == end) throw ParseNumberException(buffer);
      position_ += end - buffer.c_str();
      return ret;
    }
    Shift();
  }
  char *end;
  float ret = strtof(position_, &end);
  if (end == position_) throw ParseNumberException(ReadDelimited());
  position_ = end;
  return ret;
}

void FilePiece::SkipSpaces() throw (GZException, EndOfFileException) {
  for (; ; ++position_) {
    if (position_ == position_end_) Shift();
    if (!isspace(*position_)) return;
  }
}

const char *FilePiece::FindDelimiterOrEOF() throw (GZException, EndOfFileException) {
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

StringPiece FilePiece::ReadLine(char delim) throw (GZException, EndOfFileException) {
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
  return ret;
}

void FilePiece::Shift() throw(GZException, EndOfFileException) {
  if (at_end_) {
    progress_.Finished();
    throw EndOfFileException();
  }
  off_t desired_begin = position_ - data_.begin() + mapped_offset_;

  if (!fallback_to_read_) MMapShift(desired_begin);
  // Notice an mmap failure might set the fallback.  
  if (fallback_to_read_) ReadShift();

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
    if (desired_begin) {
      if (((off_t)-1) == lseek(*file_, desired_begin, SEEK_SET)) UTIL_THROW(ErrnoException, "mmap failed even though it worked before.  lseek failed too, so using read isn't an option either.");
    }
    // The mmap was scheduled to end the file, but now we're going to read it.  
    at_end_ = false;
    TransitionToRead();
    return;
  }
  mapped_offset_ = mapped_offset;
  position_ = data_.begin() + ignore;
  position_end_ = data_.begin() + mapped_size;

  progress_.Set(desired_begin);
}

void FilePiece::TransitionToRead() throw (GZException) {
  assert(!fallback_to_read_);
  fallback_to_read_ = true;
  data_.reset();
  data_.reset(malloc(default_map_size_), default_map_size_, scoped_memory::MALLOC_ALLOCATED);
  if (!data_.get()) UTIL_THROW(ErrnoException, "malloc failed for " << default_map_size_);
  position_ = data_.begin();
  position_end_ = position_;

#ifdef HAVE_ZLIB
  assert(!gz_file_);
  gz_file_ = gzdopen(file_.get(), "r");
  if (!gz_file_) {
    UTIL_THROW(GZException, "zlib failed to open " << file_name_);
  }
#endif
}

void FilePiece::ReadShift() throw(GZException, EndOfFileException) {
  assert(fallback_to_read_);
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

  ssize_t read_return;
#ifdef HAVE_ZLIB
  read_return = gzread(gz_file_, static_cast<char*>(data_.get()) + already_read, default_map_size_ - already_read);
  if (read_return == -1) throw GZException(gz_file_);
  if (total_size_ != kBadSize) {
    // Just get the position, don't actually seek.  Apparently this is how you do it. . . 
    off_t ret = lseek(file_.get(), 0, SEEK_CUR);
    if (ret != -1) progress_.Set(ret);
  }
#else
  read_return = read(file_.get(), static_cast<char*>(data_.get()) + already_read, default_map_size_ - already_read);
  if (read_return == -1) UTIL_THROW(ErrnoException, "read failed");
  progress_.Set(mapped_offset_);
#endif
  if (read_return == 0) {
    at_end_ = true;
  }
  position_end_ += read_return;
}

} // namespace util

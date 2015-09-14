#include "util/file_piece.hh"

#include "util/double-conversion/double-conversion.h"
#include "util/exception.hh"
#include "util/file.hh"
#include "util/mmap.hh"

#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#else
#include <unistd.h>
#endif

#include <cassert>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

namespace util {

ParseNumberException::ParseNumberException(StringPiece value) throw() {
  *this << "Could not parse \"" << value << "\" into a ";
}

// Sigh this is the only way I could come up with to do a _const_ bool.  It has ' ', '\f', '\n', '\r', '\t', and '\v' (same as isspace on C locale).
const bool kSpaces[256] = {0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

FilePiece::FilePiece(const char *name, std::ostream *show_progress, std::size_t min_buffer) :
  file_(OpenReadOrThrow(name)), total_size_(SizeFile(file_.get())), page_(SizePage()),
  progress_(total_size_, total_size_ == kBadSize ? NULL : show_progress, std::string("Reading ") + name) {
  Initialize(name, show_progress, min_buffer);
}

namespace {
std::string NamePossiblyFind(int fd, const char *name) {
  if (name) return name;
  return NameFromFD(fd);
}
} // namespace

FilePiece::FilePiece(int fd, const char *name, std::ostream *show_progress, std::size_t min_buffer) :
  file_(fd), total_size_(SizeFile(file_.get())), page_(SizePage()),
  progress_(total_size_, total_size_ == kBadSize ? NULL : show_progress, std::string("Reading ") + NamePossiblyFind(fd, name)) {
  Initialize(NamePossiblyFind(fd, name).c_str(), show_progress, min_buffer);
}

FilePiece::FilePiece(std::istream &stream, const char *name, std::size_t min_buffer) :
  total_size_(kBadSize), page_(SizePage()) {
  InitializeNoRead("istream", min_buffer);

  fallback_to_read_ = true;
  HugeMalloc(default_map_size_, false, data_);
  position_ = data_.begin();
  position_end_ = position_;

  fell_back_.Reset(stream);
}

FilePiece::~FilePiece() {}

StringPiece FilePiece::ReadLine(char delim, bool strip_cr) {
  std::size_t skip = 0;
  while (true) {
    for (const char *i = position_ + skip; i < position_end_; ++i) {
      if (*i == delim) {
        // End of line.
        // Take 1 byte off the end if it's an unwanted carriage return.
        const std::size_t subtract_cr = (
            (strip_cr && i > position_ && *(i - 1) == '\r') ?
            1 : 0);
        StringPiece ret(position_, i - position_ - subtract_cr);
        position_ = i + 1;
        return ret;
      }
    }
    if (at_end_) {
      if (position_ == position_end_) {
        Shift();
      }
      return Consume(position_end_);
    }
    skip = position_end_ - position_;
    Shift();
  }
}

bool FilePiece::ReadLineOrEOF(StringPiece &to, char delim, bool strip_cr) {
  try {
    to = ReadLine(delim, strip_cr);
  } catch (const util::EndOfFileException &e) { return false; }
  return true;
}

float FilePiece::ReadFloat() {
  return ReadNumber<float>();
}
double FilePiece::ReadDouble() {
  return ReadNumber<double>();
}
long int FilePiece::ReadLong() {
  return ReadNumber<long int>();
}
unsigned long int FilePiece::ReadULong() {
  return ReadNumber<unsigned long int>();
}

// Factored out so that istream can call this.
void FilePiece::InitializeNoRead(const char *name, std::size_t min_buffer) {
  file_name_ = name;

  default_map_size_ = page_ * std::max<std::size_t>((min_buffer / page_ + 1), 2);
  position_ = NULL;
  position_end_ = NULL;
  mapped_offset_ = 0;
  at_end_ = false;
}

void FilePiece::Initialize(const char *name, std::ostream *show_progress, std::size_t min_buffer) {
  InitializeNoRead(name, min_buffer);

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
  if ((position_end_ >= position_ + ReadCompressed::kMagicSize) && ReadCompressed::DetectCompressedMagic(position_)) {
    if (!fallback_to_read_) {
      at_end_ = false;
      TransitionToRead();
    }
  }
}

namespace {

static const double_conversion::StringToDoubleConverter kConverter(
    double_conversion::StringToDoubleConverter::ALLOW_TRAILING_JUNK | double_conversion::StringToDoubleConverter::ALLOW_LEADING_SPACES,
    std::numeric_limits<double>::quiet_NaN(),
    std::numeric_limits<double>::quiet_NaN(),
    "inf",
    "NaN");

StringPiece FirstToken(StringPiece str) {
  const char *i;
  for (i = str.data(); i != str.data() + str.size(); ++i) {
    if (kSpaces[(unsigned char)*i]) break;
  }
  return StringPiece(str.data(), i - str.data());
}

const char *ParseNumber(StringPiece str, float &out) {
  int count;
  out = kConverter.StringToFloat(str.data(), str.size(), &count);
  UTIL_THROW_IF_ARG(std::isnan(out) && str != "NaN" && str != "nan", ParseNumberException, (FirstToken(str)), "float");
  return str.data() + count;
}
const char *ParseNumber(StringPiece str, double &out) {
  int count;
  out = kConverter.StringToDouble(str.data(), str.size(), &count);
  UTIL_THROW_IF_ARG(std::isnan(out) && str != "NaN" && str != "nan", ParseNumberException, (FirstToken(str)), "double");
  return str.data() + count;
}
const char *ParseNumber(StringPiece str, long int &out) {
  char *end;
  errno = 0;
  out = strtol(str.data(), &end, 10);
  UTIL_THROW_IF_ARG(errno || (end == str.data()), ParseNumberException, (FirstToken(str)), "long int");
  return end;
}
const char *ParseNumber(StringPiece str, unsigned long int &out) {
  char *end;
  errno = 0;
  out = strtoul(str.data(), &end, 10);
  UTIL_THROW_IF_ARG(errno || (end == str.data()), ParseNumberException, (FirstToken(str)), "unsigned long int");
  return end;
}
} // namespace

template <class T> T FilePiece::ReadNumber() {
  SkipSpaces();
  while (last_space_ < position_) {
    if (UTIL_UNLIKELY(at_end_)) {
      // Hallucinate a null off the end of the file.
      std::string buffer(position_, position_end_);
      T ret;
      // Has to be null-terminated.
      const char *begin = buffer.c_str();
      const char *end = ParseNumber(StringPiece(begin, buffer.size()), ret);
      position_ += end - begin;
      return ret;
    }
    Shift();
  }
  T ret;
  position_ = ParseNumber(StringPiece(position_, last_space_ - position_), ret);
  return ret;
}

const char *FilePiece::FindDelimiterOrEOF(const bool *delim)  {
  std::size_t skip = 0;
  while (true) {
    for (const char *i = position_ + skip; i < position_end_; ++i) {
      if (delim[static_cast<unsigned char>(*i)]) return i;
    }
    if (at_end_) {
      if (position_ == position_end_) Shift();
      return position_end_;
    }
    skip = position_end_ - position_;
    Shift();
  }
}

void FilePiece::Shift() {
  if (at_end_) {
    progress_.Finished();
    throw EndOfFileException();
  }
  uint64_t desired_begin = position_ - data_.begin() + mapped_offset_;

  if (!fallback_to_read_) MMapShift(desired_begin);
  // Notice an mmap failure might set the fallback.
  if (fallback_to_read_) ReadShift();

  for (last_space_ = position_end_ - 1; last_space_ >= position_; --last_space_) {
    if (kSpaces[static_cast<unsigned char>(*last_space_)])  break;
  }
}

void FilePiece::MMapShift(uint64_t desired_begin) {
  // Use mmap.
  uint64_t ignore = desired_begin % page_;
  // Duplicate request for Shift means give more data.
  if (position_ == data_.begin() + ignore && position_) {
    default_map_size_ *= 2;
  }
  // Local version so that in case of failure it doesn't overwrite the class variable.
  uint64_t mapped_offset = desired_begin - ignore;

  uint64_t mapped_size;
  if (default_map_size_ >= static_cast<std::size_t>(total_size_ - mapped_offset)) {
    at_end_ = true;
    mapped_size = total_size_ - mapped_offset;
  } else {
    mapped_size = default_map_size_;
  }

  // Forcibly clear the existing mmap first.
  data_.reset();
  try {
    MapRead(POPULATE_OR_LAZY, *file_, mapped_offset, mapped_size, data_);
  } catch (const util::ErrnoException &e) {
    if (desired_begin) {
      SeekOrThrow(*file_, desired_begin);
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

void FilePiece::TransitionToRead() {
  assert(!fallback_to_read_);
  fallback_to_read_ = true;
  data_.reset();
  HugeMalloc(default_map_size_, false, data_);
  position_ = data_.begin();
  position_end_ = position_;

  try {
    fell_back_.Reset(file_.release());
  } catch (util::Exception &e) {
    e << " in file " << file_name_;
    throw;
  }
}

void FilePiece::ReadShift() {
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
      HugeRealloc(default_map_size_, false, data_);
      position_ = data_.begin();
      position_end_ = position_ + valid_length;
    } else {
      std::size_t moving = position_end_ - position_;
      memmove(data_.get(), position_, moving);
      position_ = data_.begin();
      position_end_ = position_ + moving;
      already_read = moving;
    }
  }

  std::size_t read_return = fell_back_.Read(static_cast<uint8_t*>(data_.get()) + already_read, default_map_size_ - already_read);
  progress_.Set(fell_back_.RawAmount());

  if (read_return == 0) {
    at_end_ = true;
  }
  position_end_ += read_return;
}

} // namespace util

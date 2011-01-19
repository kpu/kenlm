#include "util/file_piece.hh"

#include "util/exception.hh"

#include <iostream>
#include <string>
#include <limits>

#include <assert.h>
#include <ctype.h>
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
  *this << "Could not parse \"" << value << "\" into a number";
}

GZException::GZException(void *file) {
#ifdef HAVE_ZLIB
  int num;
  *this << gzerror(file, &num) << " from zlib";
#endif // HAVE_ZLIB
}

// Sigh this is the only way I could come up with to do a _const_ bool. 
const bool kIsSpace[256] = { isspace(0), isspace(1), isspace(2), isspace(3), isspace(4), isspace(5), isspace(6), isspace(7), isspace(8), isspace(9), isspace(10), isspace(11), isspace(12), isspace(13), isspace(14), isspace(15), isspace(16), isspace(17), isspace(18), isspace(19), isspace(20), isspace(21), isspace(22), isspace(23), isspace(24), isspace(25), isspace(26), isspace(27), isspace(28), isspace(29), isspace(30), isspace(31), isspace(32), isspace(33), isspace(34), isspace(35), isspace(36), isspace(37), isspace(38), isspace(39), isspace(40), isspace(41), isspace(42), isspace(43), isspace(44), isspace(45), isspace(46), isspace(47), isspace(48), isspace(49), isspace(50), isspace(51), isspace(52), isspace(53), isspace(54), isspace(55), isspace(56), isspace(57), isspace(58), isspace(59), isspace(60), isspace(61), isspace(62), isspace(63), isspace(64), isspace(65), isspace(66), isspace(67), isspace(68), isspace(69), isspace(70), isspace(71), isspace(72), isspace(73), isspace(74), isspace(75), isspace(76), isspace(77), isspace(78), isspace(79), isspace(80), isspace(81), isspace(82), isspace(83), isspace(84), isspace(85), isspace(86), isspace(87), isspace(88), isspace(89), isspace(90), isspace(91), isspace(92), isspace(93), isspace(94), isspace(95), isspace(96), isspace(97), isspace(98), isspace(99), isspace(100), isspace(101), isspace(102), isspace(103), isspace(104), isspace(105), isspace(106), isspace(107), isspace(108), isspace(109), isspace(110), isspace(111), isspace(112), isspace(113), isspace(114), isspace(115), isspace(116), isspace(117), isspace(118), isspace(119), isspace(120), isspace(121), isspace(122), isspace(123), isspace(124), isspace(125), isspace(126), isspace(127), isspace(128), isspace(129), isspace(130), isspace(131), isspace(132), isspace(133), isspace(134), isspace(135), isspace(136), isspace(137), isspace(138), isspace(139), isspace(140), isspace(141), isspace(142), isspace(143), isspace(144), isspace(145), isspace(146), isspace(147), isspace(148), isspace(149), isspace(150), isspace(151), isspace(152), isspace(153), isspace(154), isspace(155), isspace(156), isspace(157), isspace(158), isspace(159), isspace(160), isspace(161), isspace(162), isspace(163), isspace(164), isspace(165), isspace(166), isspace(167), isspace(168), isspace(169), isspace(170), isspace(171), isspace(172), isspace(173), isspace(174), isspace(175), isspace(176), isspace(177), isspace(178), isspace(179), isspace(180), isspace(181), isspace(182), isspace(183), isspace(184), isspace(185), isspace(186), isspace(187), isspace(188), isspace(189), isspace(190), isspace(191), isspace(192), isspace(193), isspace(194), isspace(195), isspace(196), isspace(197), isspace(198), isspace(199), isspace(200), isspace(201), isspace(202), isspace(203), isspace(204), isspace(205), isspace(206), isspace(207), isspace(208), isspace(209), isspace(210), isspace(211), isspace(212), isspace(213), isspace(214), isspace(215), isspace(216), isspace(217), isspace(218), isspace(219), isspace(220), isspace(221), isspace(222), isspace(223), isspace(224), isspace(225), isspace(226), isspace(227), isspace(228), isspace(229), isspace(230), isspace(231), isspace(232), isspace(233), isspace(234), isspace(235), isspace(236), isspace(237), isspace(238), isspace(239), isspace(240), isspace(241), isspace(242), isspace(243), isspace(244), isspace(245), isspace(246), isspace(247), isspace(248), isspace(249), isspace(250), isspace(251), isspace(252), isspace(253), isspace(254), isspace(255) };

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
      std::cerr << "could not close file " << file_name_ << " using zlib" << std::endl;
      abort();
    }
  }
#endif
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

float FilePiece::ReadFloat() throw(GZException, EndOfFileException, ParseNumberException) {
  return ReadNumber<float>();
}
double FilePiece::ReadDouble() throw(GZException, EndOfFileException, ParseNumberException) {
  return ReadNumber<double>();
}
long int FilePiece::ReadLong() throw(GZException, EndOfFileException, ParseNumberException) {
  return ReadNumber<long int>();
}
unsigned long int FilePiece::ReadULong() throw(GZException, EndOfFileException, ParseNumberException) {
  return ReadNumber<unsigned long int>();
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

namespace {
void ParseNumber(const char *begin, char *&end, float &out) {
#ifdef sun
  out = static_cast<float>(strtod(begin, &end));
#else
  out = strtof(begin, &end);
#endif
}
void ParseNumber(const char *begin, char *&end, double &out) {
  out = strtod(begin, &end);
}
void ParseNumber(const char *begin, char *&end, long int &out) {
  out = strtol(begin, &end, 10);
}
void ParseNumber(const char *begin, char *&end, unsigned long int &out) {
  out = strtoul(begin, &end, 10);
}
} // namespace

template <class T> T FilePiece::ReadNumber() throw(GZException, EndOfFileException, ParseNumberException) {
  SkipSpaces();
  while (last_space_ < position_) {
    if (at_end_) {
      // Hallucinate a null off the end of the file.
      std::string buffer(position_, position_end_);
      char *end;
      T ret;
      ParseNumber(buffer.c_str(), end, ret);
      if (buffer.c_str() == end) throw ParseNumberException(buffer);
      position_ += end - buffer.c_str();
      return ret;
    }
    Shift();
  }
  char *end;
  T ret;
  ParseNumber(position_, end, ret);
  if (end == position_) throw ParseNumberException(ReadDelimited());
  position_ = end;
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

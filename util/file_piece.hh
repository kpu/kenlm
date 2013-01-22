#ifndef UTIL_FILE_PIECE__
#define UTIL_FILE_PIECE__

#include "util/ersatz_progress.hh"
#include "util/exception.hh"
#include "util/file.hh"
#include "util/mmap.hh"
#include "util/read_compressed.hh"
#include "util/string_piece.hh"

#include <cstddef>
#include <iosfwd>
#include <string>

#include <stdint.h>

namespace util {

class ParseNumberException : public Exception {
  public:
    explicit ParseNumberException(StringPiece value) throw();
    ~ParseNumberException() throw() {}
};

extern const bool kSpaces[256];

// Memory backing the returned StringPiece may vanish on the next call.
class FilePiece {
  public:
    // 1 MB default.
    explicit FilePiece(const char *file, std::ostream *show_progress = NULL, std::size_t min_buffer = 1048576);
    // Takes ownership of fd.  name is used for messages.
    explicit FilePiece(int fd, const char *name = NULL, std::ostream *show_progress = NULL, std::size_t min_buffer = 1048576);

    /* Read from an istream.  Don't use this if you can avoid it.  Raw fd IO is
     * much faster.  But sometimes you just have an istream like Boost's HTTP
     * server and want to parse it the same way.
     * name is just used for messages and FileName().
     */
    explicit FilePiece(std::istream &stream, const char *name = NULL, std::size_t min_buffer = 1048576);

    ~FilePiece();

    char get() {
      if (position_ == position_end_) {
        Shift();
        if (at_end_) throw EndOfFileException();
      }
      return *(position_++);
    }

    // Leaves the delimiter, if any, to be returned by get().  Delimiters defined by isspace().
    StringPiece ReadDelimited(const bool *delim = kSpaces) {
      SkipSpaces(delim);
      return Consume(FindDelimiterOrEOF(delim));
    }

    // Unlike ReadDelimited, this includes leading spaces and consumes the delimiter.
    // It is similar to getline in that way.
    StringPiece ReadLine(char delim = '\n');

    float ReadFloat();
    double ReadDouble();
    long int ReadLong();
    unsigned long int ReadULong();

    // Skip spaces defined by isspace.
    void SkipSpaces(const bool *delim = kSpaces) {
      for (; ; ++position_) {
        if (position_ == position_end_) Shift();
        if (!delim[static_cast<unsigned char>(*position_)]) return;
      }
    }

    uint64_t Offset() const {
      return position_ - data_.begin() + mapped_offset_;
    }

    const std::string &FileName() const { return file_name_; }

  private:
    void InitializeNoRead(const char *name, std::size_t min_buffer);
    // Calls InitializeNoRead, so don't call both.
    void Initialize(const char *name, std::ostream *show_progress, std::size_t min_buffer);

    template <class T> T ReadNumber();

    StringPiece Consume(const char *to) {
      StringPiece ret(position_, to - position_);
      position_ = to;
      return ret;
    }

    const char *FindDelimiterOrEOF(const bool *delim = kSpaces);

    void Shift();
    // Backends to Shift().
    void MMapShift(uint64_t desired_begin);

    void TransitionToRead();
    void ReadShift();

    const char *position_, *last_space_, *position_end_;

    scoped_fd file_;
    const uint64_t total_size_;
    const uint64_t page_;

    std::size_t default_map_size_;
    uint64_t mapped_offset_;

    // Order matters: file_ should always be destroyed after this.
    scoped_memory data_;

    bool at_end_;
    bool fallback_to_read_;

    ErsatzProgress progress_;

    std::string file_name_;

    ReadCompressed fell_back_;
};

} // namespace util

#endif // UTIL_FILE_PIECE__

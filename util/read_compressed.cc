#include "util/read_compressed.hh"

#include "util/file.hh"
#include "util/have.hh"
#include "util/scoped.hh"

#include <algorithm>
#include <iostream>

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#ifdef HAVE_BZLIB
#include <bzlib.h>
#endif

namespace util {

CompressedException::CompressedException() throw() {}
CompressedException::~CompressedException() throw() {}

GZException::GZException() throw() {}
GZException::~GZException() throw() {}

BZException::BZException() throw() {}
BZException::~BZException() throw() {}

class ReadBase {
  public:
    virtual ~ReadBase() {}

    virtual std::size_t Read(void *to, std::size_t amount, scoped_ptr<ReadBase> &thunk) = 0;

  protected:
    ReadBase() {}
};

namespace {

// Completed file that other classes can thunk to.  
class Complete : public ReadBase {
  public:
    std::size_t Read(void *, std::size_t, scoped_ptr<ReadBase> &) {
      return 0;
    }
};

class Uncompressed : public ReadBase {
  public:
    explicit Uncompressed(int fd) : fd_(fd) {}

    std::size_t Read(void *to, std::size_t amount, scoped_ptr<ReadBase> &) {
      return PartialRead(fd_.get(), to, amount);
    }

  private:
    scoped_fd fd_;
};

class UncompressedWithHeader : public ReadBase {
  public:
    UncompressedWithHeader(int fd, void *already_data, std::size_t already_size) : fd_(fd) {
      assert(already_size);
      buf_.reset(malloc(already_size));
      if (!buf_.get()) throw std::bad_alloc();
      memcpy(buf_.get(), already_data, already_size);
      remain_ = static_cast<uint8_t*>(buf_.get());
      end_ = remain_ + already_size;
    }

    std::size_t Read(void *to, std::size_t amount, scoped_ptr<ReadBase> &thunk) {
      assert(buf_.get());
      std::size_t sending = std::min<std::size_t>(amount, end_ - remain_);
      memcpy(to, remain_, sending);
      remain_ += sending;
      if (remain_ == end_) {
        thunk.reset(new Uncompressed(fd_.release()));
      }
      return sending;
    }

  private:
    scoped_malloc buf_;
    uint8_t *remain_;
    uint8_t *end_;

    scoped_fd fd_;
};

#ifdef HAVE_ZLIB
class GZip : public ReadBase {
  private:
    static const std::size_t kInputBuffer = 16384;
  public:
    GZip(int fd, void *already_data, std::size_t already_size) 
      : file_(fd), in_buffer_(malloc(kInputBuffer)) {
      UTIL_THROW_IF(!in_buffer_.get(), ErrnoException, "Malloc failure");
      assert(already_size < kInputBuffer);
      if (already_size) {
        memcpy(in_buffer_.get(), already_data, already_size);
        stream_.next_in = static_cast<z_const Bytef *>(in_buffer_.get());
        stream_.avail_in = already_size;
        stream_.avail_in += ReadOrEOF(file_.get(), static_cast<uint8_t*>(in_buffer_.get()) + already_size, kInputBuffer - already_size);
      } else {
        stream_.avail_in = 0;
      }
      stream_.zalloc = Z_NULL;
      stream_.zfree = Z_NULL;
      stream_.opaque = Z_NULL;
      stream_.msg = NULL;
      // 32 for zlib and gzip decoding with automatic header detection.  
      // 15 for maximum window size.  
      UTIL_THROW_IF(Z_OK != inflateInit2(&stream_, 32 + 15), GZException, "Failed to initialize zlib.");
    }

    ~GZip() {
      if (Z_OK != inflateEnd(&stream_)) {
        std::cerr << "zlib could not close properly." << std::endl;
        abort();
      }
    }

    std::size_t Read(void *to, std::size_t amount, scoped_ptr<ReadBase> &thunk) {
      if (amount == 0) return 0;
      stream_.next_out = static_cast<Bytef*>(to);
      stream_.avail_out = amount;
      do {
        if (!stream_.avail_in) ReadInput();
        int result = inflate(&stream_, 0);
        switch (result) {
          case Z_OK:
            break;
          case Z_STREAM_END:
            {
              std::size_t ret = static_cast<uint8_t*>(stream_.next_out) - static_cast<uint8_t*>(to);
              thunk.reset(new Complete());
              return ret;
            }
          case Z_ERRNO:
            UTIL_THROW(ErrnoException, "zlib error");
          default:
            UTIL_THROW(GZException, "zlib encountered " << (stream_.msg ? stream_.msg : "an error ") << " code " << result);
        }
      } while (stream_.next_out == to);
      return static_cast<uint8_t*>(stream_.next_out) - static_cast<uint8_t*>(to);
    }

  private:
    void ReadInput() {
      assert(!stream_.avail_in);
      stream_.next_in = static_cast<z_const Bytef *>(in_buffer_.get());
      stream_.avail_in = ReadOrEOF(file_.get(), in_buffer_.get(), kInputBuffer);
    }

    scoped_fd file_;
    scoped_malloc in_buffer_;
    z_stream stream_;
};
#endif // HAVE_ZLIB

#ifdef HAVE_BZLIB
class BZip : public ReadBase {
  public:
    explicit BZip(int fd, void *already_data, std::size_t already_size) {
      scoped_fd hold(fd);
      closer_.reset(FDOpenOrThrow(hold));
      int bzerror = BZ_OK;
      file_ = BZ2_bzReadOpen(&bzerror, closer_.get(), 0, 0, already_data, already_size);
      switch (bzerror) {
        case BZ_OK:
          return;
        case BZ_CONFIG_ERROR:
          UTIL_THROW(BZException, "Looks like bzip2 was miscompiled.");
        case BZ_PARAM_ERROR:
          UTIL_THROW(BZException, "Parameter error");
        case BZ_IO_ERROR:
          UTIL_THROW(BZException, "IO error reading file");
        case BZ_MEM_ERROR:
          throw std::bad_alloc();
      }
    }

    ~BZip() {
      int bzerror = BZ_OK;
      BZ2_bzReadClose(&bzerror, file_);
      if (bzerror != BZ_OK) {
        std::cerr << "bz2 readclose error" << std::endl;
        abort();
      }
    }

    std::size_t Read(void *to, std::size_t amount, scoped_ptr<ReadBase> &thunk) {
      int bzerror = BZ_OK;
      int ret = BZ2_bzRead(&bzerror, file_, to, std::min<std::size_t>(static_cast<std::size_t>(INT_MAX), amount));
      switch (bzerror) {
        case BZ_STREAM_END:
          thunk.reset(new Complete());
          return ret;
        case BZ_OK:
          return ret;
        default:
          UTIL_THROW(BZException, "bzip2 error " << BZ2_bzerror(file_, &bzerror) << " code " << bzerror);
      }
    }

  private:
    scoped_FILE closer_;
    BZFILE *file_;
};
#endif // HAVE_BZLIB

ReadBase *ReadFactory(int fd) {
  scoped_fd hold(fd);
  unsigned char header[2];
  std::size_t got = ReadOrEOF(fd, header, 2);
  if (got != 2)
    return new UncompressedWithHeader(hold.release(), header, got);
  if (header[0] == 0x1f && header[1] == 0x8b) {
#ifdef HAVE_ZLIB
    return new GZip(hold.release(), header, 2);
#else
    UTIL_THROW(CompressedException, "This looks like a gzip file but gzip support was not compiled in.");
#endif
  }
  if (header[0] == 'B' && header[1] == 'Z') {
#ifdef HAVE_BZLIB
    return new BZip(hold.release(), header, 2);
#else
    UTIL_THROW(CompressedException, "This looks like a bzip file (it begins with BZ), but bzip support was not compiled in.");
#endif
  }
  try {
    AdvanceOrThrow(fd, -2);
  } catch (const util::ErrnoException &e) {
    return new UncompressedWithHeader(hold.release(), header, 2);
  }
  return new Uncompressed(hold.release());
}

} // namespace

ReadCompressed::ReadCompressed(int fd) : internal_(ReadFactory(fd)) {}

ReadCompressed::~ReadCompressed() {}

std::size_t ReadCompressed::Read(void *to, std::size_t amount) {
  return internal_->Read(to, amount, internal_);
}

} // namespace util

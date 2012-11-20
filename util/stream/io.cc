#include "util/stream/io.hh"

#include <cstring>

namespace util {
namespace stream {

ReadSizeException::ReadSizeException() throw() {}
ReadSizeException::~ReadSizeException() throw() {}

void ReadThread::Process(Block &block) {
  std::size_t got = util::ReadOrEOF(file_, block.Get(), block_size_);
  UTIL_THROW_IF(got % entry_size_, ReadSizeException, "File ended with " << got << " bytes, not a multiple of " << entry_size_ << "."); 
  if (got == 0) {
    block.SetToPoison();
  } else {
    block.SetValidSize(got);
  }
}

} // namespace stream
} // namespace util

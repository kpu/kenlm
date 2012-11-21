#include "util/stream/io.hh"

#include <cstring>

namespace util {
namespace stream {

ReadSizeException::ReadSizeException() throw() {}
ReadSizeException::~ReadSizeException() throw() {}

void Read::Run(const ChainPosition &position) {
  const std::size_t block_size = position.GetChain().BlockSize();
  const std::size_t entry_size = position.GetChain().EntrySize();
  for (Link link(position); link; ++link) {
    std::size_t got = util::ReadOrEOF(file_, link->Get(), block_size);
    UTIL_THROW_IF(got % entry_size, ReadSizeException, "File ended with " << got << " bytes, not a multiple of " << entry_size << "."); 
    if (got == 0) {
      link->SetToPoison();
      return;
    } else {
      link->SetValidSize(got);
    }
  }
}

} // namespace stream
} // namespace util

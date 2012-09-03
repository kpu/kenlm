#include "bounded/file.hh"

#include "bounded/config.hh"
#include "bounded/manager.hh"
#include "util/exception.hh"
#include "util/scoped.hh"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

namespace bounded {

int OpenTemp(const Config &config) {
  util::scoped_array<char> copy(new char[config.temporary_template.size() + 1]);
  memcpy(copy.get(), config.temporary_template.c_str(), config.temporary_template.size() + 1);
  util::scoped_fd file(mkstemp(copy.get()));
  UTIL_THROW_IF(!file.get(), util::ErrnoException, "mkstemp failed for template " << copy.get());
  UTIL_THROW_IF(unlink(copy.get()), util::ErrnoException, "unlink failed to delete " << copy.get());
  return file.release();
}

namespace {
void WriteRange(int fd, uint8_t *begin, uint8_t *end) {
  while (begin < end) {
    ssize_t wrote;
    UTIL_THROW_IF(-1 == (wrote = write(fd, begin, end - begin)), util::ErrnoException, "Failed to write " << (end - begin) << " bytes to temporary file");
    begin += wrote;
  }
}
} // namespace

void AppendAndRelease(const Chunk &chunk, std::size_t amount, Manager &manager, int fd) {
  std::size_t block = manager.GetConfig().write_block_size;
  uint8_t *begin = static_cast<uint8_t*>(chunk.Begin());
  uint8_t *end = begin + amount;

  while (begin + block < end) {
    WriteRange(fd, begin, begin + block);
    chunk.ShrinkBegin(block);
    manager.Shrink(block);
    begin += block;
  }
  WriteRange(fd, begin, end);
  chunk.ShrinkBegin(end - begin);
  manager.Shrink(end - begin);
}

void ReadOrThrow(void *to, std::size_t amount, int fd) {
  ssize_t ret;
  while (amount) {
    UTIL_THROW_IF(-1 == (ret = read(fd, to, amount)), util::ErrnoException, "Failed to read");
    UTIL_THROW_IF(!ret, util::EndOfFileException, " during read with " << amount << " bytes to go");
    amount -= ret;
  }
}

} // namespace bounded

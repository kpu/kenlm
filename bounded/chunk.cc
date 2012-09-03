#include "bounded/chunk.hh"

#include "util/exception.hh"
#include "util/mmap.hh"

#include <algorithm>
#include <iostream>

#include <assert.h>
#include <stdlib.h>

namespace bounded {

namespace {
// Block size must be a multiple of page and a multiple of the element size with target around 32 MB.  
std::size_t RoundBlockSize(const Config &config, std::size_t element_size) {
  std::size_t min_size = boost::math::lcm<std::size_t>(sysconf(_SC_PAGE_SIZE), element_size);
  return (std::max<std::size_t>(config.goal_block_size / min_size, 1)) * min_size;
}
} // namespace

Chunk::Chunk(const Config &config, std::size_t element_size) : element_size_(element_size), block_size_(RoundBlockSize(config, element_size)) {
  // We can't always mmap 0 bytes, so the first GrowEnd will call mmap.  
  base_ = NULL;
  begin_ = NULL;
  end_ = NULL;
  actual_end_ = NULL;
}

Chunk::~Chunk() {
  std::size_t amount = reinterpret_cast<uint8_t*>(actual_end_) - reinterpret_cast<uint8_t*>(begin_);
  if (amount && munmap(begin_, amount)) {
    std::cerr << "munmap failed for " << amount << " bytes at " << begin_ << std::endl;
    abort();
  }
}

void Chunk::ShrinkBegin(std::size_t amount) {
  assert(!(amount % sysconf(_SC_PAGE_SIZE)));
  UTIL_THROW_IF(munmap(begin_, amount), util::ErrnoException, "Failed to munmap " << amount);
  begin_ = reinterpret_cast<uint8_t*>(begin_) + amount;
}

void Chunk::ShrinkEnd(std::size_t amount) {
  /* We leave some cruft here because munmap requires page alignment */
  end_ = reinterpret_cast<uint8_t*>(end_) - amount;
  std::size_t page_size = sysconf(_SC_PAGE_SIZE);
  std::size_t off_page = reinterpret_cast<std::size_t>(end_) % page_size;
  uint8_t *previous_actual = reinterpret_cast<uint8_t>(actual_end_);
  actual_end_ = off_page ? end_ : (reinterpret_cast<uint8_t*>(end_) + page_size - off_page);
  std::size_t reduction = previous_actual - reinterpret_cast<uint8_t*>(actual_end_);
  if (reduction)
    UTIL_THROW_IF(munmap(actual_end_, reduction), util::ErrnoException, "Failed to munmap " << reduction << " bytes");
}

void Chunk::GrowEnd(std::size_t amount) {
  boost::unique_lock<boost::shared_mutex> lock(MappingMutex());
  std::size_t old_actual = reinterpret_cast<uint8_t*>(actual_end_) - reinterpret_cast<uint8_t*>(begin_);
  std::size_t old_fake = reinterpret_cast<uint8_t*>(end_) - reinterpret_cast<uint8_t*>(begin_);

  uint8_t *previous_begin = reinterpret_cast<uint8_t*>(begin_);
  if (begin_) {
    UTIL_THROW_IF(!(begin_ = mremap(begin_, old_actual, old_fake + amount, MREMAP_MAYMOVE)), util::ErrnoException, "Failed to mremap from " << old_actual << " to " << (old_fake + amount) << " bytes.");
  } else {
    assert(!old_fake);
    begin_ = MapAnonymous(amount);
  }
  basis_ = reinterpret_cast<uint8_t*>(basis_) + reinterpret_cast<uint8_t*>(begin_) - previous_begin;
  end_ = reinterpret_cast<uint8_t*>(begin_) + old_fake + amount;
  actual_end_ = end_;
}

void Chunk::Swap(Chunk &other) {
  std::swap(base_, other_.base_);
  mapping_.swap(other_.mapping_);
  std::swap(begin_, other_.begin_);
  std::swap(end_, other_.end_);
  std::swap(actual_end_, other_.actual_end_);
  assert(element_size_ == other.element_size_);
  assert(block_size_ == other.block_size_);
}

} // namespace bounded

#include "util/stream/chain.hh"

#include "util/exception.hh"
#include "util/pcqueue.hh"

#include <cstdlib>
#include <new>
#include <iostream>

#include <stdint.h>

namespace util {
namespace stream {

Chain::Chain(const ChainConfig &config) : config_(config), last_called_(false) {
  assert(config_.entry_size);
  // Round up to multiple of config_.entry_size.
  config_.block_size = config_.entry_size * ((config_.block_size + config_.entry_size - 1) / config_.entry_size);
  std::size_t malloc_size = config_.block_size * config_.block_count;
  memory_.reset(malloc(malloc_size));
  UTIL_THROW_IF(!memory_.get(), util::ErrnoException, "Failed to allocate " << malloc_size << " bytes for " << config_.block_count << " blocks each of size " << config_.block_size);
  // This queue has special size to accomodate all blocks.  
  queues_.push_back(new PCQueue<Block>(std::max(config_.queue_length, config_.block_size)));
  // Populate the lead queue with blocks.  
  uint8_t *base = static_cast<uint8_t*>(memory_.get());
  for (std::size_t i = 0; i < config_.block_count; ++i) {
    queues_.front().Produce(Block(base, config_.block_size));
    base += config_.block_size;
  }
}

Chain::~Chain() {
  if (!last_called_) return;
  for (std::size_t i = 0; i < config_.block_count; ++i) {
    if (!queues_.front().Consume()) return;
  }
  std::cerr << "Queue destructor without poison." << std::endl;
  abort();
}

ChainPosition Chain::Between() {
  PCQueue<Block> &in = queues_.back();
  queues_.push_back(new PCQueue<Block>(config_.queue_length));
  return ChainPosition(in, queues_.back(), this);
}

ChainPosition Chain::Last() {
  UTIL_THROW_IF(last_called_, util::Exception, "Last() called twice");
  last_called_ = true;
  return ChainPosition(queues_.back(), queues_.front(), this);
}

Link::Link(const ChainPosition &position) : in_(position.in_), out_(position.out_) {
  in_->Consume(current_);
}

Link::~Link() {
  if (current_) {
    std::cerr << "Last input should have been poison." << std::endl;
    std::abort();
  } else {
    // Pass the poison!
    out_->Produce(current_);
  }
}

Link &Link::operator++() {
  assert(current_);
  out_->Produce(current_);
  in_->Consume(current_);
  return *this;
}

} // namespace stream
} // namespace util

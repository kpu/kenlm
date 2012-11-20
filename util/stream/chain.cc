#include "util/stream/chain.hh"

#include "util/exception.hh"
#include "util/pcqueue.hh"

#include <cstdlib>
#include <new>
#include <iostream>

#include <stdint.h>

namespace util {
namespace stream {

Chain::Chain(const ChainConfig &config) : config_(config), running_(false) {
  assert(config_.entry_size);
  // Round up to multiple of config_.entry_size.
  config_.block_size = config_.entry_size * ((config_.block_size + config_.entry_size - 1) / config_.entry_size);
  std::size_t malloc_size = config_.block_size * config_.block_count;
  memory_.reset(malloc(malloc_size));
  UTIL_THROW_IF(!memory_.get(), util::ErrnoException, "Failed to allocate " << malloc_size << " bytes for " << config_.block_count << " blocks each of size " << config_.block_size);
  // This queue has special size to accomodate all blocks.  
  queues_.push_back(new PCQueue<Block>(std::max(config_.queue_length, config_.block_size)));
}

Chain::~Chain() {
  if (running_) {
    for (std::size_t i = 0; i < config_.block_count; ++i) {
      if (!queues_.front().Consume()) return;
    }
    std::cerr << "Chain destructor called before posion was passed through." << std::endl;
    std::abort();
  }
}

void Chain::StartRunning() {
  queues_.pop_back();
  running_ = true;

  // Populate the lead queue with blocks.  
  uint8_t *base = static_cast<uint8_t*>(memory_.get());
  for (std::size_t i = 0; i < config_.block_count; ++i) {
    queues_.front().Produce(Block(base, config_.block_size));
    base += config_.block_size;
  }
}

util::PCQueue<Block> *ChainPosition::RunningGetOutQueue() {
  assert(chain_->running_);
  return &chain_->queues_[(chain_->queues_.size() == index_ + 1) ? 0 : (index_ + 1)];
}

ChainPosition::ChainPosition(Chain &chain) : chain_(&chain), index_(chain.queues_.size() - 1) {
  chain_->queues_.push_back(new PCQueue<Block>(chain_->config_.queue_length));
}

Link::Link(ChainPosition &position) : in_(position.GetInQueue()) {
  in_->Consume(current_);
  // This has to be done after consuming, so that we know whether the queue is the beginning of the chain or not.  
  out_ = position.RunningGetOutQueue();
}

Link::~Link() {
  if (!current_.Get()) {
    // Never initialized, something probably went wrong during setup.
    return;
  }
  if (current_) {
    std::cerr << "Last input should have been poison." << std::endl;
    std::abort();
  } else {
    // Pass the poison!
    out_->Produce(current_);
  }
}

Link &Link::operator++() {
  out_->Produce(current_);
  in_->Consume(current_);
  return *this;
}

} // namespace stream
} // namespace util

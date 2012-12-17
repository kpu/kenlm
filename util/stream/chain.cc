#include "util/stream/chain.hh"

#include "util/stream/io.hh"

#include "util/exception.hh"
#include "util/pcqueue.hh"

#include <cstdlib>
#include <new>
#include <iostream>

#include <stdint.h>

namespace util {
namespace stream {

ChainConfigException::ChainConfigException() throw() { *this << "Chain configured with "; }
ChainConfigException::~ChainConfigException() throw() {}

void Recycler::Run(const ChainPosition &position) {
  for (Link l(position); l; ++l) {
    l->SetValidSize(position.GetChain().BlockSize());
  }
}

const Recycler kRecycle = Recycler();

Chain::Chain(const ChainConfig &config) : config_(config), complete_called_(false) {
  UTIL_THROW_IF(!config.entry_size, ChainConfigException, "zero-size entries.");
  UTIL_THROW_IF(!config.block_size, ChainConfigException, "block size zero");
  UTIL_THROW_IF(!config.block_count, ChainConfigException, "block count zero");
  // Round up to multiple of config_.entry_size.
  config_.block_size = config_.entry_size * ((config_.block_size + config_.entry_size - 1) / config_.entry_size);
}

Chain::~Chain() {
  Wait();
}

ChainPosition Chain::Add() {
  if (!Running()) Start();
  PCQueue<Block> &in = queues_.back();
  queues_.push_back(new PCQueue<Block>(config_.block_count));
  return ChainPosition(in, queues_.back(), this);
}

Chain &Chain::operator>>(const WriteAndRecycle &writer) {
  threads_.push_back(new Thread(Complete(), writer));
  return *this;
}

void Chain::Wait(bool release_memory) {
  if (queues_.empty()) {
    assert(threads_.empty());
    return; // Nothing to wait for.  
  }
  if (!complete_called_) CompleteLoop();
  threads_.clear();
  for (std::size_t i = 0; queues_.front().Consume(); ++i) {
    if (i == config_.block_count) {
      std::cerr << "Chain ending without poison." << std::endl;
      abort();
    }
  }
  queues_.clear();
  complete_called_ = false;
  if (release_memory) memory_.reset();
}

void Chain::Start() {
  Wait(false);
  if (!memory_.get()) {
    // Allocate memory.  
    assert(threads_.empty());
    assert(queues_.empty());
    std::size_t malloc_size = config_.block_size * config_.block_count;
    memory_.reset(malloc(malloc_size));
    UTIL_THROW_IF(!memory_.get(), util::ErrnoException, "Failed to allocate " << malloc_size << " bytes for " << config_.block_count << " blocks each of size " << config_.block_size);
  }
  // This queue can accomodate all blocks.    
  queues_.push_back(new PCQueue<Block>(config_.block_size));
  // Populate the lead queue with blocks.  
  uint8_t *base = static_cast<uint8_t*>(memory_.get());
  for (std::size_t i = 0; i < config_.block_count; ++i) {
    queues_.front().Produce(Block(base, config_.block_size));
    base += config_.block_size;
  }
}

ChainPosition Chain::Complete() {
  assert(Running());
  UTIL_THROW_IF(complete_called_, util::Exception, "CompleteLoop() called twice");
  complete_called_ = true;
  return ChainPosition(queues_.back(), queues_.front(), this);
}

Link::Link() : in_(NULL), out_(NULL), poisoned_(true) {}

void Link::Init(const ChainPosition &position) {
  UTIL_THROW_IF(in_, util::Exception, "Link::Init twice");
  in_ = position.in_;
  out_ = position.out_;
  poisoned_ = false;
  in_->Consume(current_);
}

Link::Link(const ChainPosition &position) : in_(NULL) {
  Init(position);
}

Link::~Link() {
  if (current_) {
    std::cerr << "Last input should have been poison." << std::endl;
    std::abort();
  } else {
    if (!poisoned_) {
      // Pass the poison!
      out_->Produce(current_);
    }
  }
}

Link &Link::operator++() {
  assert(current_);
  out_->Produce(current_);
  in_->Consume(current_);
  if (!current_) {
    poisoned_ = true;
    out_->Produce(current_);
  }
  return *this;
}

void Link::Poison() {
  assert(!poisoned_);
  current_.SetToPoison();
  out_->Produce(current_);
  poisoned_ = true;
}

} // namespace stream
} // namespace util

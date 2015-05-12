#include "util/stream/rewindable_stream.hh"
#include "util/pcqueue.hh"

namespace util {
namespace stream {

RewindableStream::RewindableStream()
    : current_(NULL), in_(NULL), out_(NULL), poisoned_(true) {
  // nothing
}

void RewindableStream::Init(const ChainPosition &position) {
  UTIL_THROW_IF2(in_, "RewindableStream::Init twice");
  in_ = position.in_;
  out_ = position.out_;
  poisoned_ = false;
  progress_ = position.progress_;
  entry_size_ = position.GetChain().EntrySize();
  block_size_ = position.GetChain().BlockSize();
  FetchBlock();
  current_bl_ = &second_bl_;
  current_ = static_cast<uint8_t*>(current_bl_->Get());
  end_ = current_ + current_bl_->ValidSize();
}

const void *RewindableStream::Get() const {
  return current_;
}

void *RewindableStream::Get() {
  return current_;
}

RewindableStream &RewindableStream::operator++() {
  assert(*this);
  assert(current_ < end_);
  current_ += entry_size_;
  if (current_ == end_) {
    // two cases: either we need to fetch the next block, or we've already
    // fetched it before. We can check this by looking at the current_bl_
    // pointer: if it's at the second_bl_, we need to flush and fetch a new
    // block. Otherwise, we can just move over to the second block.
    if (current_bl_ == &second_bl_) {
      if (first_bl_) {
        out_->Produce(first_bl_);
        progress_ += first_bl_.ValidSize();
      }
      first_bl_ = second_bl_;
      FetchBlock();
    }
    current_bl_ = &second_bl_;
    current_ = static_cast<uint8_t *>(second_bl_.Get());
    end_ = current_ + second_bl_.ValidSize();
  }

  if (!*current_bl_)
  {
    if (current_bl_ == &second_bl_ && first_bl_)
    {
      out_->Produce(first_bl_);
      progress_ += first_bl_.ValidSize();
    }
    out_->Produce(*current_bl_);
    poisoned_ = true;
  }

  return *this;
}

void RewindableStream::FetchBlock() {
  // The loop is needed since it is *feasible* that we're given 0 sized but
  // valid blocks
  do {
    in_->Consume(second_bl_);
  } while (second_bl_ && second_bl_.ValidSize() == 0);
}

void RewindableStream::Mark() {
  marked_ = current_;
}

void RewindableStream::Rewind() {
  if (marked_ >= first_bl_.Get() && marked_ < first_bl_.ValidEnd()) {
    current_bl_ = &first_bl_;
    current_ = marked_;
  } else if (marked_ >= second_bl_.Get() && marked_ < second_bl_.ValidEnd()) {
    current_bl_ = &second_bl_;
    current_ = marked_;
  } else { UTIL_THROW2("RewindableStream rewound too far"); }
}

void RewindableStream::Poison() {
  assert(!poisoned_);

  // Three things: if we have a buffered first block, we need to produce it
  // first. Then, produce the partial "current" block, and then send the
  // poison down the chain

  // if we still have a buffered first block, produce it first
  if (current_bl_ == &second_bl_ && first_bl_) {
    out_->Produce(first_bl_);
    progress_ += first_bl_.ValidSize();
  }

  // send our partial block
  current_bl_->SetValidSize(current_
                            - static_cast<uint8_t *>(current_bl_->Get()));
  out_->Produce(*current_bl_);
  progress_ += current_bl_->ValidSize();

  // send down the poison
  current_bl_->SetToPoison();
  out_->Produce(*current_bl_);
  poisoned_ = true;
}
}
}

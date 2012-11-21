#ifndef UTIL_STREAM_STREAM__
#define UTIL_STREAM_STREAM__

#include "util/stream/chain.hh"

#include <boost/noncopyable.hpp>

#include <assert.h>
#include <stdint.h>

namespace util {
namespace stream {

class Stream : boost::noncopyable {
  public:
    explicit Stream(const ChainPosition &position, bool generating) : 
        entry_size_(position.GetChain().EntrySize()),
        block_size_(position.GetChain().BlockSize()),
        generating_(generating),
        block_it_(position) {
      current_ = static_cast<uint8_t*>(block_it_->Get());
      if (generating_) block_it_->SetValidSize(block_size_);
      end_ = current_ + block_it_->ValidSize();
    }

    ~Stream() {
      if (generating_) Poison();
    }

    operator bool() const { return current_ != NULL; }
    bool operator!() const { return current_ == NULL; }

    const void *Get() const { return current_; }
    void *Get() { return current_; }

    void Poison() {
      generating_ = false;
      block_it_->SetValidSize(current_ - static_cast<uint8_t*>(block_it_->Get()));
      ++block_it_;
      block_it_.Poison();
    }
    
    Stream &operator++() {
      assert(*this);
      assert(current_ < end_);
      current_ += entry_size_;
      while (current_ == end_ && current_) {
        ++block_it_;
        current_ = static_cast<uint8_t*>(block_it_->Get());
        if (generating_) block_it_->SetValidSize(block_size_);
        end_ = current_ + block_it_->ValidSize();
      }  
      return *this;
    }

  private:
    uint8_t *current_, *end_;

    const std::size_t entry_size_;
    const std::size_t block_size_;

    bool generating_;

    Link block_it_;
};

} // namespace stream
} // namespace util
#endif // UTIL_STREAM_STREAM__

#ifndef UTIL_STREAM_CHAIN__
#define UTIL_STREAM_CHAIN__

#include "util/stream/block.hh"
#include "util/scoped.hh"

#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/thread/thread.hpp>

#include <cstddef>

#include <assert.h>

namespace util {
template <class T> class PCQueue;
namespace stream {

struct ChainConfig {
  std::size_t entry_size;
  // Chain's constructor will make thisa multiple of entry_size. 
  std::size_t block_size;
  std::size_t block_count;
  std::size_t queue_length;
};

class Chain {
  public:
    explicit Chain(const ChainConfig &config);

    ~Chain();

    std::size_t EntrySize() const {
      return config_.entry_size;
    }
    std::size_t BlockSize() const {
      return config_.block_size;
    }

    void StartRunning();

  private:
    friend class ChainPosition;

    ChainConfig config_;

    scoped_malloc memory_;

    boost::ptr_vector<PCQueue<Block> > queues_;

    bool running_;
};

// Create this in the main thread to properly synchronize position assignment.  
class ChainPosition {
  public:
    explicit ChainPosition(Chain &chain);

    Chain &MutableChain() { return *chain_; }

  private:
    friend class Link;
    util::PCQueue<Block> *GetInQueue() {
      return &chain_->queues_[index_];
    }
    util::PCQueue<Block> *RunningGetOutQueue();

    Chain *chain_;
    std::size_t index_;
};

// Create the link in the worker thread using the position token.
class Link {
  public:
    explicit Link(ChainPosition &position);

    ~Link();

    Block &operator*() { return current_; }
    const Block &operator*() const { return current_; }

    Block *operator->() { return &current_; }
    const Block *operator->() const { return &current_; }

    Link &operator++();

    operator bool() const { return current_; }

  private:
    Block current_;
    PCQueue<Block> *in_, *out_;
};

template <class Child> class LinkThread {
  public:
    ~LinkThread() {
      thread_.join();
    }

    void operator()() {
      for (Link link(position_); link; ++link) {
        static_cast<Child*>(this)->Process(*link);
        if (!link) break;
      }
    }

  protected:
    // position_ is constructed in the main thread to preserve order.  
    explicit LinkThread(Chain &chain) : position_(chain), thread_(boost::ref(*this)) {}

  private:
    ChainPosition position_;
    boost::thread thread_;
};

} // namespace stream
} // namespace util

#endif // UTIL_STREAM_CHAIN__

#ifndef UTIL_STREAM_CHAIN__
#define UTIL_STREAM_CHAIN__

#include "util/stream/block.hh"
#include "util/stream/config.hh"
#include "util/stream/multi_progress.hh"
#include "util/scoped.hh"

#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/thread/thread.hpp>

#include <cstddef>

#include <assert.h>

namespace util {
template <class T> class PCQueue;
namespace stream {

class ChainConfigException : public Exception {
  public:
    ChainConfigException() throw();
    ~ChainConfigException() throw();
};

class Chain;
// Specifies position in chain for Link constructor.
class ChainPosition {
  public:
    const Chain &GetChain() const { return *chain_; }
  private:
    friend class Chain;
    friend class Link;
    ChainPosition(PCQueue<Block> &in, PCQueue<Block> &out, Chain *chain, MultiProgress &progress) 
      : in_(&in), out_(&out), chain_(chain), progress_(progress.Add()) {}

    PCQueue<Block> *in_, *out_;

    Chain *chain_;

    WorkerProgress progress_;
};

// Position is usually ChainPosition but if there are multiple streams involved, this can be ChainPositions.  
class Thread {
  public:
    template <class Position, class Worker> Thread(const Position &position, const Worker &worker)
      : thread_(boost::ref(*this), position, worker) {}

    ~Thread();

    template <class Position, class Worker> void operator()(const Position &position, Worker &worker) {
      try {
        worker.Run(position);
      } catch (const std::exception &e) {
        UnhandledException(e);
      }
    }

  private:
    void UnhandledException(const std::exception &e);

    boost::thread thread_;
};

class Recycler {
  public:
    void Run(const ChainPosition &position);
};

extern const Recycler kRecycle;
class WriteAndRecycle;

class Chain {
  private:
    template <class T, void (T::*ptr)(const ChainPosition &) = &T::Run> struct CheckForRun {
      typedef Chain type;
    };

  public:
    explicit Chain(const ChainConfig &config);

    ~Chain();

    void ActivateProgress() {
      assert(!Running());
      progress_.Activate();
    }

    void SetProgressTarget(uint64_t target) {
      progress_.SetTarget(target);
    }

    std::size_t EntrySize() const {
      return config_.entry_size;
    }
    std::size_t BlockSize() const {
      return block_size_;
    }

    // Two ways to add to the chain: Add() or operator>>.  
    ChainPosition Add();

    // This is for adding threaded workers with a Run method.  
    template <class Worker> typename CheckForRun<Worker>::type &operator>>(const Worker &worker) {
      assert(!complete_called_);
      threads_.push_back(new Thread(Add(), worker));
      return *this;
    }

    // Avoid copying the worker.  
    template <class Worker> typename CheckForRun<Worker>::type &operator>>(const boost::reference_wrapper<Worker> &worker) {
      assert(!complete_called_);
      threads_.push_back(new Thread(Add(), worker));
      return *this;
    }

    // Note that Link and Stream also define operator>> outside this class.  

    // To complete the loop, call CompleteLoop(), >> kRecycle, or the destructor.  
    void CompleteLoop() {
      threads_.push_back(new Thread(Complete(), kRecycle));
    }

    Chain &operator>>(const Recycler &) {
      CompleteLoop();
      return *this;
    }

    Chain &operator>>(const WriteAndRecycle &writer);

    // Chains are reusable.  Call Wait to wait for everything to finish and free memory.  
    void Wait(bool release_memory = true);

    // Waits for the current chain to complete (if any) then starts again.  
    void Start();

    bool Running() const { return !queues_.empty(); }

  private:
    ChainPosition Complete();

    ChainConfig config_;

    std::size_t block_size_;

    scoped_malloc memory_;

    boost::ptr_vector<PCQueue<Block> > queues_;

    bool complete_called_;

    boost::ptr_vector<Thread> threads_;

    MultiProgress progress_;
};

// Create the link in the worker thread using the position token.
class Link {
  public:
    // Either default construct and Init or just construct all at once.
    Link();
    void Init(const ChainPosition &position);

    explicit Link(const ChainPosition &position);

    ~Link();

    Block &operator*() { return current_; }
    const Block &operator*() const { return current_; }

    Block *operator->() { return &current_; }
    const Block *operator->() const { return &current_; }

    Link &operator++();

    operator bool() const { return current_; }

    void Poison();

  private:
    Block current_;
    PCQueue<Block> *in_, *out_;
 
    bool poisoned_;

    WorkerProgress progress_;
};

inline Chain &operator>>(Chain &chain, Link &link) {
  link.Init(chain.Add());
  return chain;
}

} // namespace stream
} // namespace util

#endif // UTIL_STREAM_CHAIN__

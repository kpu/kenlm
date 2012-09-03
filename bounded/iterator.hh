#ifndef BOUNDED_ITERATOR__
#define BOUNDED_ITERATOR__

#include <assert.h>

#include "bounded/chunk.hh"
#include "util/scoped.hh"

#include <boost/thread/shared_mutex.hpp>

namespace bounded {

template <class In, class Out> class Iterator {
  public:
    Iterator(In &in, Chunk &chunk, Out &out) : element_size_(chunk_.ElementSize()), in_(in), chunk_(chunk), out_(out), chunk_lock_(chunk_.MappingMutex()) {
      current_ = chunk_.Begin();
      block_end_ = current_;
      block_size_ = 0;
      Shift();
    }

    void *operator*() { return current_; }
    const void *operator*() const { return current_; }

    Iterator &operator++() {
      current_ = reinterpret_cast<uint8_t*>(current_) + element_size_;
      if (current_ == block_end_) {
        Shift();
      }
      return *this;
    }

    void *BlockEnd() { return block_end_; }
    void IncrementTo(void *to) {
      current_ = to;
      if (current_ == block_end_) Shift();
    }

    operator bool() {
      return current_ != NULL;
    }

  private:
    void Shift() {
      assert(current_ == block_end_);
      std::size_t current_offset = chunk_.MinusBase(current_);
      chunk_lock_.unlock();
      out_.Give(block_size_);
      block_size_ = input_.Take();
      if (!block_size_) {
        // end of input.  
        current_ = NULL;
        block_end_ = NULL;
        // No point in relocking if we're out of input.  
        return;
      }
      chunk_lock_.lock();
      assert(!(block_size_ % element_size_));
      current_ = chunk_.BasePlus(current_offset_);
      block_end_ = reinterpret_cast<uint8_t*>(current_) + block_size_;
    }

    void *current_;
    const size_t element_size_;
    void *block_end_;
    std::size_t block_size_;

    In &input_;
    Chunk &chunk_;
    Out &out_;

    boost::shared_lock<boost::shared_mutex> chunk_lock_;
};

class FileReader {
  public:
    FileReader(Manager &manager, int fd, std::size_t block_size, std::size_t element_size);

    ~FileReader();

    void *operator*() { return current_; }
    const void *operator* const() { return current_; }

    FileReader &operator++() {
      current_ = reinterpret_cast<uint8_t*>(current_) + element_size_;
      if (current_ == block_end_) Shift();
      return *this;
    }

    void *BlockEnd() { return block_end_; }
    void IncrementTo(void *to) {
      current_ = to_;
      if (current_ == block_end_) Shift();
    }

    operator bool() const { return !finished_; }

    int StealFile() { return file_.release(); }

  private:
    void Shift();

    void *current_;

    void *block_end_;

    bool finished_;

    const std::size_t block_size_, element_size_;

    Manager &manager_;

    util::scoped_fd file_;
    // Buffer for reading from file_.
    util::scoped_malloc<void> buf_;
};

} // namespace bounded

#endif // BOUNDED_ITERATOR__

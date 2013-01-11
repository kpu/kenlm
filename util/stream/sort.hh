/* Usage:
 * Sort<Compare> sorter(temp, compare);
 * Chain(config) >> Read(file) >> sorter.Unsorted();
 * Stream stream;
 * Chain chain(config) >> sorter.Sorted(internal_config, lazy_config) >> stream;
 * 
 * Note that sorter must outlive any threads that use Unsorted or Sorted.  
 *
 * Combiners take the form:
 * bool operator()(void *into, const void *option, const Compare &compare) const
 * which returns true iff a combination happened.  The sorting algorithm
 * guarantees compare(into, option).  But it does not guarantee 
 * compare(option, into).  
 * Currently, combining is only done in merge steps, not during on-the-fly
 * sort.  Use a hash table for that.  
 */

#ifndef UTIL_STREAM_SORT__
#define UTIL_STREAM_SORT__

#include "util/stream/chain.hh"
#include "util/stream/config.hh"
#include "util/stream/io.hh"
#include "util/stream/stream.hh"
#include "util/stream/timer.hh"

#include "util/file.hh"
#include "util/sized_iterator.hh"

#include <algorithm>
#include <iostream>
#include <queue>
#include <string>

namespace util {
namespace stream {

struct NeverCombine {
  template <class Compare> bool operator()(const void *, const void *, const Compare &) const { 
    return false;
  }
};

// Manage the offsets of sorted blocks in a file.  
class Offsets {
  public:
    explicit Offsets(int fd) : log_(fd) {
      Reset();
    }

    int File() const { return log_; }

    void Append(uint64_t length) {
      if (!length) return;
      ++block_count_;
      if (length == cur_.length) {
        ++cur_.run;
        return;
      }
      WriteOrThrow(log_, &cur_, sizeof(Entry));
      cur_.length = length;
      cur_.run = 1;
    }

    void FinishedAppending() {
      WriteOrThrow(log_, &cur_, sizeof(Entry));
      SeekOrThrow(log_, sizeof(Entry)); // Skip 0,0 at beginning.
      cur_.run = 0;
      if (block_count_) {
        ReadOrThrow(log_, &cur_, sizeof(Entry));
        assert(cur_.length);
        assert(cur_.run);
      }
    }

    uint64_t RemainingBlocks() const { return block_count_; }

    uint64_t TotalOffset() const { return output_sum_; }

    uint64_t PeekSize() const {
      return cur_.length;
    }

    uint64_t NextSize() {
      assert(block_count_);
      uint64_t ret = cur_.length;
      output_sum_ += ret;

      --cur_.run;
      --block_count_;
      if (!cur_.run && block_count_) {
        ReadOrThrow(log_, &cur_, sizeof(Entry));
        assert(cur_.length);
        assert(cur_.run);
      }
      return ret;
    }

    void Reset() {
      SeekOrThrow(log_, 0);
      ResizeOrThrow(log_, 0);
      cur_.length = 0;
      cur_.run = 0;
      block_count_ = 0;
      output_sum_ = 0;
    }

  private:
    int log_;

    struct Entry {
      uint64_t length;
      uint64_t run;
    };
    Entry cur_;

    uint64_t block_count_;

    uint64_t output_sum_;
};

// A priority queue of entries backed by file buffers
template <class Compare> class MergeQueue {
  public:
    MergeQueue(int fd, std::size_t buffer_size, std::size_t entry_size, const Compare &compare)
      : queue_(Greater(compare)), in_(fd), buffer_size_(buffer_size), entry_size_(entry_size) {}

    void Push(void *base, uint64_t offset, uint64_t amount) {
      queue_.push(Entry(base, in_, offset, amount, buffer_size_));
    }

    const void *Top() const {
      return queue_.top().Current();
    }

    void Pop() {
      Entry top(queue_.top());
      queue_.pop();
      if (top.Increment(in_, buffer_size_, entry_size_))
        queue_.push(top);
    }

    std::size_t Size() const {
      return queue_.size();
    }

    bool Empty() const {
      return queue_.empty();
    }

  private:
    // Priority queue contains these entries.  
    class Entry {
      public:
        Entry() {}

        Entry(void *base, int fd, uint64_t offset, uint64_t amount, std::size_t buf_size) {
          offset_ = offset;
          remaining_ = amount;
          buffer_end_ = static_cast<uint8_t*>(base) + buf_size;
          Read(fd, buf_size);
        }

        bool Increment(int fd, std::size_t buf_size, std::size_t entry_size) {
          current_ += entry_size;
          if (current_ != buffer_end_) return true;
          return Read(fd, buf_size);
        }

        const void *Current() const { return current_; }

      private:
        bool Read(int fd, std::size_t buf_size) {
          current_ = buffer_end_ - buf_size;
          std::size_t amount;
          if (static_cast<uint64_t>(buf_size) < remaining_) {
            amount = buf_size;
          } else if (!remaining_) {
            return false;
          } else {
            amount = remaining_;
            buffer_end_ = current_ + remaining_;
          }
          PReadOrThrow(fd, current_, amount, offset_);
          offset_ += amount;
          assert(current_ <= buffer_end_);
          remaining_ -= amount;
          return true;
        }

        // Buffer
        uint8_t *current_, *buffer_end_;
        // File
        uint64_t remaining_, offset_;
    };

    // Wrapper comparison function for queue entries.   
    class Greater : public std::binary_function<const Entry &, const Entry &, bool> {
      public:
        explicit Greater(const Compare &compare) : compare_(compare) {}

        bool operator()(const Entry &first, const Entry &second) const {
          return compare_(second.Current(), first.Current());
        }

      private:
        const Compare compare_;
    };

    typedef std::priority_queue<Entry, std::vector<Entry>, Greater> Queue;
    Queue queue_;

    const int in_;
    const std::size_t buffer_size_;
    const std::size_t entry_size_;
};

/* A worker object that merges.  If the number of pieces to merge exceeds the
 * arity, it outputs multiple sorted blocks, recording to out_offsets.  
 * However, users will only every see a single sorted block out output because
 * Sort::Sorted insures the arity is higher than the number of pieces before
 * returning this.   
 */
template <class Compare, class Combine> class MergingReader {
  public:
    MergingReader(int in, Offsets *in_offsets, Offsets *out_offsets, std::size_t buffer_size, std::size_t total_memory, const Compare &compare, const Combine &combine) :
        compare_(compare), combine_(combine),
        in_(in),
        in_offsets_(in_offsets), out_offsets_(out_offsets),
        buffer_size_(buffer_size), total_memory_(total_memory) {}

    void Run(const ChainPosition &position) {
      Run(position, false);
    }

    void Run(const ChainPosition &position, bool assert_one) {
      // Special case: nothing to read.  
      if (!in_offsets_->RemainingBlocks()) {
        Link l(position);
        l.Poison();
        return;
      }
      // If there's just one entry, just read.
      if (in_offsets_->RemainingBlocks() == 1) {
        // Sequencing is important.
        uint64_t offset = in_offsets_->TotalOffset();
        uint64_t amount = in_offsets_->NextSize();
        ReadSingle(offset, amount, position);
        if (out_offsets_) out_offsets_->Append(amount);
        return;
      }

      Stream str(position);
      scoped_malloc buffer(malloc(total_memory_));
      UTIL_THROW_IF(!buffer.get(), ErrnoException, " while trying to malloc " << total_memory_ << " bytes");
      uint8_t *const buffer_end = static_cast<uint8_t*>(buffer.get()) + total_memory_;

      const std::size_t entry_size = position.GetChain().EntrySize();

      while (in_offsets_->RemainingBlocks()) {
        // Use bigger buffers if there's less remaining.
        uint64_t per_buffer = std::max(buffer_size_, total_memory_ / in_offsets_->RemainingBlocks());
        per_buffer -= per_buffer % entry_size;
        assert(per_buffer);

        // Populate queue.
        MergeQueue<Compare> queue(in_, per_buffer, entry_size, compare_);
        for (uint8_t *buf = static_cast<uint8_t*>(buffer.get()); 
            in_offsets_->RemainingBlocks() && (buf + std::min(per_buffer, in_offsets_->PeekSize()) <= buffer_end);) {
          uint64_t offset = in_offsets_->TotalOffset();
          uint64_t size = in_offsets_->NextSize();
          queue.Push(buf, offset, size);
          buf += static_cast<std::size_t>(std::min<uint64_t>(size, per_buffer));
        }
        // This shouldn't happen but it's probably better to die than loop indefinitely.
        if (queue.Size() < 2 && in_offsets_->RemainingBlocks()) {
          std::cerr << "Bug in sort implementation: not merging at least two stripes." << std::endl;
          abort();
        }
        if (assert_one && in_offsets_->RemainingBlocks()) {
          std::cerr << "Bug in sort implementation: should only be one merge group for lazy sort" << std::endl;
          abort();
        }

        uint64_t written = 0;
        // Merge including combiner support.  
        memcpy(str.Get(), queue.Top(), entry_size);
        for (queue.Pop(); !queue.Empty(); queue.Pop()) {
          if (!combine_(str.Get(), queue.Top(), compare_)) {
            ++written; ++str;
            memcpy(str.Get(), queue.Top(), entry_size);
          }
        }
        ++written; ++str;
        if (out_offsets_)
          out_offsets_->Append(written * entry_size);
      }
      str.Poison();
    }

  private: 
    void ReadSingle(uint64_t offset, const uint64_t size, const ChainPosition &position) {
      // Special case: only one to read.  
      const uint64_t end = offset + size;
      const uint64_t block_size = position.GetChain().BlockSize();
      Link l(position);
      for (; offset + block_size < end; ++l, offset += block_size) {
        PReadOrThrow(in_, l->Get(), block_size, offset);
        l->SetValidSize(block_size);
      }
      PReadOrThrow(in_, l->Get(), end - offset, offset);
      l->SetValidSize(end - offset);
      (++l).Poison();
      return;
    }
   
    Compare compare_;
    Combine combine_;

    int in_;

  protected:
    Offsets *in_offsets_;

  private:
    Offsets *out_offsets_;
 
    std::size_t buffer_size_;
    std::size_t total_memory_;
};

// The lazy step owns the remaining files.  This keeps track of them.  
template <class Compare, class Combine> class OwningMergingReader : public MergingReader<Compare, Combine> {
  private:
    typedef MergingReader<Compare, Combine> P;
  public:
    OwningMergingReader(int data, const Offsets &offsets, const SortConfig &config, const Compare &compare, const Combine &combine) 
      : P(data, NULL, NULL, config.buffer_size, config.lazy_total_memory, compare, combine),
        data_(data),
        offsets_(offsets) {}

    void Run(const ChainPosition &position) {
      P::in_offsets_ = &offsets_;
      scoped_fd data(data_);
      scoped_fd offsets_file(offsets_.File());
      P::Run(position, true);
    }

  private:
    int data_;
    Offsets offsets_;
};

// Don't use this directly.  Worker that sorts blocks.   
template <class Compare> class BlockSorter {
  public:
    BlockSorter(Offsets &offsets, const Compare &compare) :
      offsets_(&offsets), compare_(compare) {}

    void Run(const ChainPosition &position) {
      const std::size_t entry_size = position.GetChain().EntrySize();
      for (Link link(position); link; ++link) {
        // Record the size of each block in a separate file.    
        offsets_->Append(link->ValidSize());
        void *end = static_cast<uint8_t*>(link->Get()) + link->ValidSize();
        std::sort(
            SizedIt(link->Get(), entry_size),
            SizedIt(end, entry_size),
            compare_);
      }
      offsets_->FinishedAppending();
    }

  private:
    Offsets *offsets_;
    SizedCompare<Compare> compare_;
};

template <class Compare, class Combine> void MergeSort(const SortConfig &config, scoped_fd &data, scoped_fd &offsets_file, Offsets &offsets, const Compare &compare, const Combine &combine) {
  scoped_fd data2(MakeTemp(config.temp_prefix));
  int fd_in = data.get(), fd_out = data2.get();
  scoped_fd offsets2_file(MakeTemp(config.temp_prefix));
  Offsets offsets2(offsets2_file.get());
  Offsets *offsets_in = &offsets, *offsets_out = &offsets2;

  // Double buffered writing.  
  ChainConfig chain_config;
  chain_config.entry_size = config.entry_size;
  chain_config.block_count = 2;
  chain_config.total_memory = config.buffer_size * 2;
  Chain chain(chain_config);

  const uint64_t lazy_arity = std::max<uint64_t>(1, config.lazy_total_memory / config.buffer_size);
  while (offsets_in->RemainingBlocks() > lazy_arity) {
    uint64_t size = SizeOrThrow(fd_in);
    if (size < static_cast<uint64_t>(config.lazy_total_memory)) return;
    std::size_t reading_memory = config.total_memory - 2 * config.buffer_size;
    if (size < static_cast<uint64_t>(reading_memory)) {
      reading_memory = static_cast<std::size_t>(size);
    }
    SeekOrThrow(fd_in, 0);
    chain >>
      MergingReader<Compare, Combine>(
          fd_in,
          offsets_in, offsets_out,
          config.buffer_size,
          reading_memory,
          compare, combine) >>
      WriteAndRecycle(fd_out);
    chain.Wait();
    offsets_out->FinishedAppending();
    ResizeOrThrow(fd_in, 0);
    offsets_in->Reset();
    std::swap(fd_in, fd_out);
    std::swap(offsets_in, offsets_out);
  }
  SeekOrThrow(fd_in, 0);
  if (fd_in == data2.get()) {
    data.reset(data2.release());
    offsets_file.reset(offsets2_file.release());
    offsets = offsets2;
  }
}

class BadSortConfig : public Exception {
  public:
    BadSortConfig() throw() {}
    ~BadSortConfig() throw() {}
};

template <class Compare, class Combine = NeverCombine> class Sort {
  public:
    Sort(Chain &in, const SortConfig &config, const Compare &compare = Compare(), const Combine &combine = Combine())
      : config_(config),
        data_(MakeTemp(config.temp_prefix)),
        offsets_file_(MakeTemp(config.temp_prefix)), offsets_(offsets_file_.get()),
        compare_(compare), combine_(combine) {
      config_.entry_size = in.EntrySize();
      UTIL_THROW_IF(!config_.entry_size, BadSortConfig, "Sorting entries of size 0");
      // Make buffer_size a multiple of the entry_size.  
      config_.buffer_size -= config_.buffer_size % config_.entry_size;
      UTIL_THROW_IF(!config_.buffer_size, BadSortConfig, "Sort buffer too small");
      UTIL_THROW_IF(config_.total_memory < config_.buffer_size * 4, BadSortConfig, "Sorting memory " << config_.total_memory << " is too small for four buffers (two read and two write).");
      in >> BlockSorter<Compare>(offsets_, compare_) >> WriteAndRecycle(data_.get());
    }

    int StealCompleted() {
      // Force lazy arity 1.
      config_.lazy_total_memory = 0;
      MergeSort(config_, data_, offsets_file_, offsets_, compare_, combine_);
      SeekOrThrow(data_.get(), 0);
      offsets_file_.reset();
      return data_.release();
    }

    uint64_t Size() const {
      return SizeOrThrow(data_.get());
    }

    void Output(Chain &out, std::ostream *progress = NULL) {
      MergeSort(config_, data_, offsets_file_, offsets_, compare_, combine_);
      out.SetProgressTarget(Size());
      out >> OwningMergingReader<Compare, Combine>(data_.get(), offsets_, config_, compare_, combine_);
      data_.release();
      offsets_file_.release();
    }

  private:
    SortConfig config_;

    scoped_fd data_;

    scoped_fd offsets_file_;
    Offsets offsets_;

    const Compare compare_;
    const Combine combine_;
};

// returns bytes to be read on demand.  
template <class Compare, class Combine> uint64_t BlockingSort(Chain &chain, const SortConfig &config, const Compare &compare = Compare(), const Combine &combine = NeverCombine()) {
  Sort<Compare, Combine> sorter(chain, config, compare, combine);
  chain.Wait(true);
  uint64_t size = sorter.Size();
  sorter.Output(chain);
  return size;
}

} // namespace stream
} // namespace util

#endif // UTIL_STREAM_SORT__

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
    }

    uint64_t RemainingBlocks() const { return block_count_; }

    uint64_t TotalOffset() const { return output_sum_; }

    // Throws EndOfFileException if out.
    uint64_t NextSize() {
      assert(RemainingBlocks());
      if (!cur_.run) {
        ReadOrThrow(log_, &cur_, sizeof(Entry));
        assert(cur_.length);
      }
      --cur_.run;
      --block_count_;
      output_sum_ += cur_.length;
      assert(cur_.length);
      return cur_.length;
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

/* A worker object that merges.  If the number of pieces to merge exceeds the
 * arity, it outputs multiple sorted blocks, recording to out_offsets.  
 * However, users will only every see a single sorted block out output because
 * Sort::Sorted insures the arity is higher than the number of pieces before
 * returning this.   
 */
template <class Compare, class Combine> class MergingReader {
  public:
    MergingReader(int in, Offsets *in_offsets, Offsets *out_offsets, std::size_t arity, std::size_t buffer_size, const Compare &compare, const Combine &combine) :
        compare_(compare), combine_(combine),
        in_(in),
        in_offsets_(in_offsets), out_offsets_(out_offsets),
        arity_(arity), buffer_size_(buffer_size) {}

    void Run(const ChainPosition &position) {
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
      const std::size_t entry_size = position.GetChain().EntrySize();
      const std::size_t be_multiple = arity_ * entry_size;
      const std::size_t total_buffer = be_multiple * ((buffer_size_ + be_multiple - 1) / be_multiple);
      scoped_malloc buffer(malloc(total_buffer));
      UTIL_THROW_IF(!buffer.get(), ErrnoException, " while trying to malloc " << total_buffer << " bytes");

      while (in_offsets_->RemainingBlocks()) {
        std::size_t arity = std::min<uint64_t>(static_cast<uint64_t>(arity_), in_offsets_->RemainingBlocks());
        //  The buffer size was calculated to support full arity, but the practical arity may be lower.  
        std::size_t per_buffer = total_buffer / arity;
        per_buffer -= per_buffer % entry_size;
        assert(per_buffer);

        // Populate queue.
        uint8_t *buf = static_cast<uint8_t*>(buffer.get());
        QueueGreater inner_compare(compare_);
        Queue queue(inner_compare);
        for (std::size_t i = 0; i < arity; ++i, buf += per_buffer) {
          QueueEntry entry;
          entry.offset = in_offsets_->TotalOffset();
          entry.remaining = in_offsets_->NextSize();
          assert(entry.remaining);
          assert(!(entry.remaining % entry_size));
          // current is set relative to end by Read. 
          entry.buffer_end = buf + per_buffer;
          // entries has only non-empty streams, so this is always true.  
          entry.Read(in_, per_buffer);
          assert(entry.current < entry.buffer_end);
          queue.push(entry);
        }

        uint64_t written = 0;

        assert(!queue.empty());
        // Write the first one for the combiner.  
        {
          QueueEntry top(queue.top());
          queue.pop();
          memcpy(str.Get(), top.current, entry_size);
          if (top.Increment(in_, per_buffer, entry_size))
            queue.push(top);
        }

        while (!queue.empty()) {
          QueueEntry top(queue.top());
          queue.pop();
          if (!combine_(str.Get(), top.current, compare_)) {
            ++written; ++str;
            memcpy(str.Get(), top.current, entry_size);
          }
          if (top.Increment(in_, per_buffer, entry_size))
            queue.push(top);
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

    struct QueueEntry {
      bool Read(int fd, std::size_t buf_size) {
        current = buffer_end - buf_size;
        std::size_t amount;
        if (static_cast<uint64_t>(buf_size) < remaining) {
          amount = buf_size;
        } else {
          amount = remaining;
          buffer_end = current + remaining;
          if (!remaining) return false;
        }
        PReadOrThrow(fd, current, amount, offset);
        offset += amount;
        assert(current <= buffer_end);
        remaining -= amount;
        return true;
      }

      bool Increment(int fd, std::size_t buf_size, std::size_t entry_size) {
        current += entry_size;
        if (current != buffer_end) return true;
        return Read(fd, buf_size);
      }

      // Buffer
      uint8_t *current, *buffer_end;
      // File
      uint64_t remaining, offset;
    };

    class QueueGreater : public std::binary_function<const QueueEntry &, const QueueEntry &, bool> {
      public:
        explicit QueueGreater(const Compare &compare) : compare_(compare) {}

        bool operator()(const QueueEntry &first, const QueueEntry &second) const {
          return compare_(second.current, first.current);
        }

      private:
        const Compare compare_;
    };

    typedef std::priority_queue<QueueEntry, std::vector<QueueEntry>, QueueGreater> Queue;
    
    Compare compare_;
    Combine combine_;

    int in_;

  protected:
    Offsets *in_offsets_;

  private:
    Offsets *out_offsets_;

    std::size_t arity_;
    
    std::size_t buffer_size_;
};

// The lazy step owns the remaining files.  This keeps track of them.  
template <class Compare, class Combine> class OwningMergingReader : public MergingReader<Compare, Combine> {
  private:
    typedef MergingReader<Compare, Combine> P;
  public:
    OwningMergingReader(int data, const Offsets &offsets, const SortConfig &config, const Compare &compare, const Combine &combine) 
      : P(data, NULL, NULL, config.lazy_arity, config.lazy_total_read_buffer, compare, combine),
        data_(data),
        offsets_(offsets) {}

    void Run(const ChainPosition &position) {
      P::in_offsets_ = &offsets_;
      scoped_fd data(data_);
      scoped_fd offsets_file(offsets_.File());
      P::Run(position);
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
  Chain chain(config.chain);
  while (offsets_in->RemainingBlocks() > config.lazy_arity) {
    SeekOrThrow(fd_in, 0);
    chain >>
      MergingReader<Compare, Combine>(
          fd_in,
          offsets_in, offsets_out,
          config.arity,
          config.total_read_buffer,
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

template <class Compare, class Combine = NeverCombine> class Sort {
  public:
    Sort(Chain &in, const SortConfig &config, const Compare &compare = Compare(), const Combine &combine = Combine())
      : config_(config),
        data_(MakeTemp(config.temp_prefix)),
        offsets_file_(MakeTemp(config.temp_prefix)), offsets_(offsets_file_.get()),
        compare_(compare), combine_(combine) {

      UTIL_THROW_IF(config.arity < 2, Exception, "Cannot have an arity < 2.");
      UTIL_THROW_IF(config.lazy_arity == 0, Exception, "Cannot have lazy arity 0.");
      config_.chain.entry_size = in.EntrySize();
      in >> BlockSorter<Compare>(offsets_, compare_) >> WriteAndRecycle(data_.get());
    }

    int StealCompleted() {
      config_.lazy_arity = 1;
      MergeSort(config_, data_, offsets_file_, offsets_, compare_, combine_);
      SeekOrThrow(data_.get(), 0);
      offsets_file_.reset();
      return data_.release();
    }

    // returns the total number of bytes written
    uint64_t Output(Chain &out) {
      MergeSort(config_, data_, offsets_file_, offsets_, compare_, combine_);
      uint64_t byte_count = util::SizeOrThrow(data_.get());
      out >> OwningMergingReader<Compare, Combine>(data_.get(), offsets_, config_, compare_, combine_);
      data_.release();
      offsets_file_.release();
      return byte_count;
    }

  private:
    SortConfig config_;

    scoped_fd data_;

    scoped_fd offsets_file_;
    Offsets offsets_;

    const Compare compare_;
    const Combine combine_;
};

// returns total bytes written
template <class Compare, class Combine> uint64_t BlockingSort(Chain &in, Chain &out, const SortConfig &config, const Compare &compare = Compare(), const Combine &combine = NeverCombine(), const std::string &timer_name = "") {
  Sort<Compare, Combine> sorter(in, config, compare, combine);
  in.Wait(true);

  UTIL_TIMER("(%w s) Finished partial (non-lazy) merge sort for " + timer_name + "\n");
  return sorter.Output(out);
}

// returns total bytes written
template <class Compare, class Combine> uint64_t BlockingSort(Chain &chain, const SortConfig &config, const Compare &compare = Compare(), const Combine &combine = NeverCombine(), const std::string &timer_name = "") {
  return BlockingSort(chain, chain, config, compare, combine, timer_name);
}

// returns total bytes written
template <class Compare> uint64_t BlockingSort(Chain &chain, const SortConfig &config, const Compare &compare = Compare(), const std::string& timer_name = "") {
  return BlockingSort(chain, config, compare, NeverCombine(), timer_name);
}

} // namespace stream
} // namespace util

#endif // UTIL_STREAM_SORT__

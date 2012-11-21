#ifndef UTIL_STREAM_SORT__
#define UTIL_STREAM_SORT__

#include "util/stream/chain.hh"
#include "util/stream/io.hh"
#include "util/stream/stream.hh"

#include "util/file.hh"
#include "util/sized_iterator.hh"

#include <algorithm>
#include <queue>

namespace util {
namespace stream {

// Manage the offsets of sorted blocks in a file.  
class Offsets {
  public:
    explicit Offsets(const TempMaker &temp) : log_(temp.Make()) {
      Reset();
    }

    void Append(uint64_t length) {
      if (!length) return;
      ++block_count_;
      if (length == cur_.length) {
        ++cur_.run;
        return;
      }
      WriteOrThrow(log_.get(), &cur_, sizeof(Entry));
      cur_.length = length;
      cur_.run = 1;
    }

    void FinishedAppending() {
      WriteOrThrow(log_.get(), &cur_, sizeof(Entry));
      SeekOrThrow(log_.get(), sizeof(Entry)); // Skip 0,0 at beginning.
      cur_.run = 0;
    }

    uint64_t RemainingBlocks() const { return block_count_; }

    uint64_t TotalOffset() const { return output_sum_; }

    // Throws EndOfFileException if out.
    uint64_t NextSize() {
      assert(RemainingBlocks());
      if (!cur_.run) {
        ReadOrThrow(log_.get(), &cur_, sizeof(Entry));
        assert(cur_.length);
      }
      --cur_.run;
      --block_count_;
      output_sum_ += cur_.length;
      assert(cur_.length);
      return cur_.length;
    }

    void Reset() {
      SeekOrThrow(log_.get(), 0);
      ResizeOrThrow(log_.get(), 0);
      cur_.length = 0;
      cur_.run = 0;
      block_count_ = 0;
      output_sum_ = 0;
    }

  private:
    scoped_fd log_;

    struct Entry {
      uint64_t length;
      uint64_t run;
    };
    Entry cur_;

    uint64_t block_count_;

    uint64_t output_sum_;
};

template <class Compare> class Sort;

template <class Compare> class MergingReader {
  public:
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
        out_offsets_->Append(amount);
        return;
      }

      Stream str(position, true);
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
        uint64_t total_to_write = 0;
        Queue queue(compare_);
        for (std::size_t i = 0; i < arity; ++i, buf += per_buffer) {
          QueueEntry entry;
          entry.offset = in_offsets_->TotalOffset();
          entry.remaining = in_offsets_->NextSize();
          assert(entry.remaining);
          assert(!(entry.remaining % entry_size));
          total_to_write += entry.remaining;
          // current is set relative to end by Read. 
          entry.buffer_end = buf + per_buffer;
          // entries has only non-empty streams, so this is always true.  
          entry.Read(in_, per_buffer);
          assert(entry.current < entry.buffer_end);
          queue.push(entry);
        }
        out_offsets_->Append(total_to_write);

        while (!queue.empty()) {
          QueueEntry top(queue.top());
          queue.pop();
          memcpy(str.Get(), top.current, entry_size);
          ++str;
          top.current += entry_size;
          assert(top.current <= top.buffer_end);
          if (top.current != top.buffer_end || top.Read(in_, per_buffer))
            queue.push(top);
        }
      }
      str.Poison();
    }

  private:
    friend class Sort<Compare>;
    MergingReader(int in, Offsets &in_offsets, Offsets &out_offsets, std::size_t arity, std::size_t buffer_size, const Compare &compare) :
        compare_(compare), in_(in), in_offsets_(&in_offsets), out_offsets_(&out_offsets), arity_(arity), buffer_size_(buffer_size) {}

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
      // Buffer
      uint8_t *current, *buffer_end;
      // File
      uint64_t remaining, offset;
    };

    class QueueGreater : public std::binary_function<const QueueEntry &, const QueueEntry &, bool> {
      public:
        QueueGreater(const Compare &compare) : compare_(compare) {}

        bool operator()(const QueueEntry &first, const QueueEntry &second) const {
          return compare_(second.current, first.current);
        }

      private:
        const Compare compare_;
    };

    typedef std::priority_queue<QueueEntry, std::vector<QueueEntry>, QueueGreater> Queue;
    
    QueueGreater compare_;

    int in_;

    Offsets *in_offsets_;
    Offsets *out_offsets_;

    std::size_t arity_;
    
    std::size_t buffer_size_;
};

template <class Compare> class SortChain;

struct MergeConfig {
  std::size_t arity;
  // Shared across all arity readers.  
  std::size_t total_read_buffer;
  // Configuration for the chain from reader to file writer.
  ChainConfig chain;
};

/* Usage:
 * Sort<Compare> sorter(temp, compare);
 * {
 *   Chain chain(config);
 *   // Add stuff to chain here.
 *
 *   SortChain<Compare> on_chain(chain, sorter);
 * }
 * int sorted_fd = sorter.Merge(sort_config);
 */
template <class Compare> class Sort {
  public:
    explicit Sort(const TempMaker &temp, const Compare &compare) :
        temp_(temp), data_(temp_.Make()), offsets_(temp), compare_(compare), called_(false) {}

    int Merge(const MergeConfig &config) {
      UTIL_THROW_IF(!called_, Exception, "Sort::Merge called without preparing the chain with SortChain.");
      scoped_fd data2(temp_.Make());
      int fd_in = data_.get(), fd_out = data2.get();
      Offsets offsets2(temp_);
      Offsets *offsets_in = &offsets_, *offsets_out = &offsets2;
      while (offsets_in->RemainingBlocks() > 1) {
        SeekOrThrow(fd_in, 0);
        {
          Chain chain(config.chain);
          Thread<MergingReader<Compare> > reader(
              chain.Between(), 
              MergingReader<Compare>(
                fd_in,
                *offsets_in, *offsets_out,
                config.arity,
                config.total_read_buffer,
                compare_));
          Thread<Write> write(chain.Last(), fd_out);
        }
        offsets_out->FinishedAppending();
        ResizeOrThrow(fd_in, 0);
        offsets_in->Reset();
        std::swap(fd_in, fd_out);
        std::swap(offsets_in, offsets_out);
      }
      SeekOrThrow(fd_in, 0);
      return (fd_in == data_.get()) ? data_.release() : data2.release();
    }

  private:
    friend class SortChain<Compare>;

    TempMaker temp_;
    scoped_fd data_;

    Offsets offsets_;

    const Compare compare_;

    bool called_;
};

/* Munch a chain, writing sorted blocks in preparation for the merge step. */
template <class Compare> class SortChain {
  public:
    SortChain(Chain &chain, Sort<Compare> &boss) :
        sort_(chain.Between(), SortWorker(boss.offsets_, boss.compare_)),
        write_(chain.Last(), boss.data_.get()) {
      // The boss only likes to be called once.  
      assert(!boss.called_);
      boss.called_ = true;
    }    
        
  private:
    class SortWorker {
      public:
        SortWorker(Offsets &offsets, const Compare &compare) :
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

    Thread<SortWorker> sort_;
    Thread<Write> write_;
};

} // namespace stream
} // namespace util

#endif // UTIL_STREAM_SORT__

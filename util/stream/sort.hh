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

#include "util/file.hh"
#include "util/sized_iterator.hh"

#include <algorithm>
#include <queue>

namespace util {
namespace stream {

template <class Compare, class Combine> class Sort;
template <class Compare> class UnsortedRet;
template <class Compare> Chain &operator>>(Chain &chain, UnsortedRet<Compare> info);

// Manage the offsets of sorted blocks in a file.  
class Offsets {
  public:
    explicit Offsets(const std::string &temp) : log_(MakeTemp(temp)) {
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

    void MoveFrom(Offsets &other) {
      log_.reset(other.log_.release());
      cur_ = other.cur_;
      block_count_ = other.block_count_;
      output_sum_ = other.output_sum_;
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

/* A worker object that merges.  If the number of pieces to merge exceeds the
 * arity, it outputs multiple sorted blocks, recording to out_offsets.  
 * However, users will only every see a single sorted block out output because
 * Sort::Sorted insures the arity is higher than the number of pieces before
 * returning this.   
 */
template <class Compare, class Combine> class MergingReader {
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
        uint64_t total_to_write = 0;
        QueueGreater inner_compare(compare_);
        Queue queue(inner_compare);
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
        if (out_offsets_) out_offsets_->Append(total_to_write);

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
            ++str;
            memcpy(str.Get(), top.current, entry_size);
          }
          if (top.Increment(in_, per_buffer, entry_size))
            queue.push(top);
        }
        ++str;
      }
      str.Poison();
    }

  private:
    friend class Sort<Compare, Combine>;
    MergingReader(int in, Offsets &in_offsets, Offsets *out_offsets, std::size_t arity, std::size_t buffer_size, const Compare &compare, const Combine &combine) :
        compare_(compare), combine_(combine),
        in_(in),
        in_offsets_(&in_offsets), out_offsets_(out_offsets),
        arity_(arity), buffer_size_(buffer_size) {}

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

    Offsets *in_offsets_;
    Offsets *out_offsets_;

    std::size_t arity_;
    
    std::size_t buffer_size_;
};

// Returned by Sort<Compare>::Write.  Users normally don't care to see this;
// just do
// chain >> sorter.Write();
template <class Compare> class UnsortedRet {
  private:
    template <class A, class B> friend class Sort;
    // Yep, that says >> <> since it's a template function.  
    friend Chain &operator>> <> (Chain &chain, UnsortedRet<Compare> info);
    UnsortedRet(int data, Offsets &offsets, const Compare &compare, std::size_t &set_entry_size) 
      : data_(data), offsets_(&offsets), compare_(&compare), set_entry_size_(&set_entry_size) {}

    int data_;
    Offsets *offsets_;
    const Compare *compare_;
    std::size_t *set_entry_size_;
};

// Don't use this directly.  Get it from Sort::Unsorted.
template <class Compare> class UnsortedWorker {
  public:
    UnsortedWorker(Offsets &offsets, const Compare &compare) :
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

template <class Compare> Chain &operator>>(Chain &chain, UnsortedRet<Compare> info) {
  // Write the entry size into sort's config.  
  *info.set_entry_size_ = chain.EntrySize();
  return chain >> UnsortedWorker<Compare>(*info.offsets_, *info.compare_) >> Write(info.data_) >> kRecycle;
}

struct NeverCombine {
  template <class Compare> bool operator()(const void *, const void *, const Compare &) const { 
    return false;
  }
};

template <class Compare, class Combine = NeverCombine> class Sort {
  public:
    explicit Sort(const SortConfig &config, const Compare &compare = Compare(), const Combine &combine = Combine()) :
        config_(config),
        data_(MakeTemp(config.temp_prefix)),
        offsets_(config.temp_prefix),
        compare_(compare),
        combine_(combine),
        written_(false) {}

    UnsortedRet<Compare> Unsorted() {
      written_ = true;
      // This will get the entry size from the incoming information.  
      return UnsortedRet<Compare>(data_.get(), offsets_, compare_, config_.chain.entry_size);
    }

    MergingReader<Compare, Combine> Sorted() {
      InternalLoop(config_.lazy_arity);
      return MergingReader<Compare, Combine>(data_.get(), offsets_, NULL, config_.lazy_arity, config_.lazy_total_read_buffer, compare_, combine_);
    }

    // Releases ownership of fd, so caller should take it over.  
    int CompletelySorted() {
      InternalLoop(1);
      return data_.release();
    }

  private:
    void InternalLoop(std::size_t lazy_arity) {
      UTIL_THROW_IF(!written_, Exception, "Sort::Sorted called before Sort::Unsorted.");
      UTIL_THROW_IF(config_.arity < 2, Exception, "Cannot have an arity < 2.");
      UTIL_THROW_IF(lazy_arity == 0, Exception, "Cannot have lazy arity 0.");
      scoped_fd data2(MakeTemp(config_.temp_prefix));
      int fd_in = data_.get(), fd_out = data2.get();
      Offsets offsets2(config_.temp_prefix);
      Offsets *offsets_in = &offsets_, *offsets_out = &offsets2;
      while (offsets_in->RemainingBlocks() > lazy_arity) {
        SeekOrThrow(fd_in, 0);
        Chain(config_.chain) >>
          MergingReader<Compare, Combine>(
              fd_in,
              *offsets_in, offsets_out,
              config_.arity,
              config_.total_read_buffer,
              compare_, combine_) >>
          Write(fd_out);
        offsets_out->FinishedAppending();
        ResizeOrThrow(fd_in, 0);
        offsets_in->Reset();
        std::swap(fd_in, fd_out);
        std::swap(offsets_in, offsets_out);
      }
      SeekOrThrow(fd_in, 0);
      if (fd_in == data2.get()) {
        data_.reset(data2.release());
        offsets_.MoveFrom(offsets2);
      }
    }

    SortConfig config_;

    scoped_fd data_;

    Offsets offsets_;

    const Compare compare_;
    const Combine combine_;

    bool written_;
};

} // namespace stream
} // namespace util

#endif // UTIL_STREAM_SORT__

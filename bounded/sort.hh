#ifndef BOUNDED_SORT__
#define BOUNDED_SORT__

#include "bounded/chunk.hh"
#include "bounded/input.hh"
#include "bounded/iterator.hh"
#include "bounded/output.hh"

#include "util/scoped.hh"
#include "util/sized_iterator.hh"

#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/utility/base_from_member.hpp>

#include <algorithm>
#include <functional>
#include <queue>
#include <vector>

namespace bounded {

namespace detail {

class SortBase : public KeepOutput {
  protected:
    SortBase(Manager &manager, Chunk &chunk);

    virtual ~SortBase();

    virtual std::size_t RunSort(void *end) const = 0;

    void DumpAndShrink(std::size_t amount);

    Input *FinishBackend();

  private:
    class File : boost::noncopyable {
      public:
        File(const Config &config, std::size_t size);

        int get() const { return f_.get(); }

        int release() { return f_.release(); }

        std::size_t Size() const { return size_; }

      private:
        util::scoped_fd f_;
        std::size_t size_;
    };

    struct FileGreaterBySize : public std::binary_function<const boost::shared_ptr<File> &, const boost::shared_ptr<File> &, bool> {
      bool operator()(const boost::shared_ptr<File> &first, const boost::shared_ptr<File> &second) const {
        return first->Size() > second->Size();
      }
    };

    // Priority queue by size with lowest size at the top.  Silly STL with your copyable container requirements.  
    std::priority_queue<boost::shared_ptr<File>, std::vector<boost::shared_ptr<File> >, FileGreaterBySize> queue_;
};

template <class Iter> std::size_t JustCopy(Iter &iter, uint8_t *out, uint8_t *goal) {
  for (uint8_t *it = out; it < goal;) {
    if (!iter) return it - out;
    std::size_t batch = std::min<std::size_t>((uint8_t*)iter.BlockEnd() - (uint8_t*)*iter, goal - it);
    memcpy(it, *iter, batch);
    iter.IncrementTo((uint8_t*)*iter + batch);
    it += batch;
  }
  return it - out;
}

template <class Compare, class Merger, class Iter1, class Iter2> std::size_t Merge(const Compare &compare, const Combine &combine, Iter1 &iter1, Iter2 &iter2, uint8_t *out, uint8_t *goal, std::size_t element_size) {
  if (!iter1) return JustCopy(iter2, out, goal);
  if (!iter2) return JustCopy(iter1, out, goal);
  for (uint8_t *it = out; it < goal;) {
    if (compare(*iter1, *iter2)) {
      memcpy(it, iter1, element_size);
      it += element_size;
      if (!++iter1) return (it - out) + JustCopy(iter2, it, goal);
    } else if (combine.TestAndCombine(*iter1, *iter2, it)) {
      it += element_size;
      if (!++iter1) return (it - out) + JustCopy(iter2, it, goal);
      if (!++iter2) return (it - out) + JustCopy(iter1, it, goal);
    } else {
      memcpy(it, iter2, element_size);
      it += element_size;
      if (!++iter2) return (it - out) + JustCopy(iter1, it, goal);
    }
  }
  return it - out;
}

} // namespace detail

// Merge memory with a file.  
template <class Compare, class Combine> class MergeInput : private boost::base_from_member<Chunk>, public Input {
  // Base from member oddities.  
  typedef boost::base_from_member<Chunk> pbase;
  Chunk &SwapOut(Chunk &with) {
    pbase::member.swap(with);
    return with;
  }
  public:
    // swap_me comes in as the memory to be merged.  This class takes ownership, placing it in member.  It substitutes the chunk that will be merged into.  
    MergeInput(const Compare &compare, const Combine &combine, Manager &manager, Chunk &swap_me, int on_disk) :
      pbase(manager.GetConfig(), swap_me.ElementSize()),
      Input(SwapOut(swap_me)),
      // Now chunk_ is where we're writing and member is the memory to merge.  
      compare_(compare), combine_(combine),
      disk_(manager, on_disk, chunk_.BlockSize(), chunk_.ElementSize()),
      mem_input_(manager, member),
      discard_(manager, member),
      mem_(mem_input_, member, discard_),
      writing_offset_(0), allocated_(0) {}

    std::size_t Take() {
      if (!disk && !mem_) return 0;
      if (allocated_ < chunk_.BlockSize()) {
        chunk_.GrowEnd(chunk_.BlockSize() - allocated_);
        allocated_ = chunk_.BlockSize();
      }
      boost::shared_lock<boost::mutex> lock(chunk_.MappingMutex());
      uint8_t *out = reinterpret_cast<uint8_t*>(chunk_.BasePlus(writing_offset_));
      uint8_t *goal = out + chunk_.BlockSize();
      std::size_t merged = detail::Merge<Compare, Combine, FileReader, Iterator<MemoryInput, DiscardOutput> >(compare_, disk_, mem_, out, goal, chunk_.ElementSize());
      writing_offset_ = chunk_.MinusBase(out + merged);
      allocated_ -= merged;
      if (!disk_ && !mem_) {
        chunk_.ShrinkEnd(chunk_allocated);
        manager_.Shrink(chunk_allocated);
      }
      return merged;
    }

  private:
    const Compare compare_;
    const Combine combine_;

    FileReader disk_;

    MemoryInput mem_input_;
    DiscardOutput discard_;

    Iterator<MemoryInput, DiscardOutput> mem_;

    std::size_t writing_offset_, allocated_;
};

template <class Compare, class Combine> class Sort : public detail::SortBase {
  public:
    Sort(Manager &manager, Chunk &chunk, const Compare &compare = Compare(), const Combine &combine = Combine()) 
      : SortBase(manager, element_size), compare_(compare), combine_(combine) {
      manager.AddSpiller(*this);
    }

    ~Sort() { manager_.RemoveSpiller(*this); }

  private:
    std::size_t RunSort(void *end_void) {
      std::sort(util::SizedIt(chunk_.Begin(), element_size_), util::SizedIt(end_void, element_size_), compare_);
      uint8_t *const begin = reinterpret_cast<uint8_t*>(chunk_.Begin());
      uint8_t *const end = reinterpret_cast<uint8_t*>(end_void);
      if (!combine_.ActuallyCombines()) return end - begin;
      // The rest is here is combining.  
      uint8_t *to = begin;
      uint8_t *from = to + element_size_;
      for (uint8_t *from = to + element_size_; from < end; from += element_size_) {
        if (!combine_.TestAndCombine(from, to)) {
          to += element_size_;
          if (to != from) memcpy(to, from, element_size_);
        }
      }
      return to + element_size_;
    }

    const util::SizedCompare<Compare> compare_;
    const Combine combine_;
};

} // namespace bounded

#endif // BOUNDED_SORT__

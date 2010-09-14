#ifndef UTIL_SORTED_UNIFORM__
#define UTIL_SORTED_UNIFORM__

#include <algorithm>
#include <cstddef>

#include <assert.h>
#include <inttypes.h>

namespace util {

inline std::size_t Pivot(uint64_t off, uint64_t range, std::size_t width) {
  std::size_t ret = static_cast<std::size_t>(static_cast<float>(off) / static_cast<float>(range) * static_cast<float>(width));
  // Cap for floating point rounding
  return (ret < width) ? ret : width - 1;
}
/*inline std::size_t Pivot(uint32_t off, uint32_t range, std::size_t width) {
  return static_cast<std::size_t>(static_cast<uint64_t>(off) * static_cast<uint64_t>(width) / static_cast<uint64_t>(range));
}
inline std::size_t Pivot(uint16_t off, uint16_t range, std::size_t width) {
  return static_cast<std::size_t>(static_cast<std::size_t>(off) * width / static_cast<std::size_t>(range));
}
inline std::size_t Pivot(unsigned char off, unsigned char range, std::size_t width) {
  return static_cast<std::size_t>(static_cast<std::size_t>(off) * width / static_cast<std::size_t>(range));
}*/

template <class Iterator, class Key> bool SortedUniformFind(Iterator begin, Iterator end, const Key key, Iterator &out) {
  if (begin == end) return false;
  Key below(begin->GetKey());
  if (key <= below) {
    if (key == below) { out = begin; return true; }
    return false;
  }
  // Make the range [begin, end].  
  --end;
  Key above(end->GetKey());
  if (key >= above) {
    if (key == above) { out = end; return true; }
    return false;
  }

  // Search the range [begin + 1, end - 1] knowing that *begin == below, *end == above.  
  while (end - begin > 1) {
    Iterator pivot(begin + (1 + Pivot(key - below, above - below, static_cast<std::size_t>(end - begin - 1))));
    Key mid(pivot->GetKey());
    if (mid < key) {
      begin = pivot;
      below = mid;
    } else if (mid > key) {
      end = pivot;
      above = mid;
    } else {
      out = pivot;
      return true;
    }
  }
  return false;
}

// To use this template, you need to define a Pivot function to match Key.  
template <class PackingT> class SortedUniformMap {
  public:
    typedef PackingT Packing;
    typedef typename Packing::ConstIterator ConstIterator;

  public:
    // Offer consistent API with probing hash.
    static std::size_t Size(std::size_t entries, float ignore = 0.0) {
      return sizeof(uint64_t) + entries * Packing::kBytes;
    }

    SortedUniformMap() 
#ifdef DEBUG
      : initialized_(false), loaded_(false) 
#endif
    {}

    SortedUniformMap(void *start, std::size_t allocated) : 
      begin_(Packing::FromVoid(reinterpret_cast<uint64_t*>(start) + 1)),
      end_(begin_), size_ptr_(reinterpret_cast<uint64_t*>(start)) 
#ifdef DEBUG
      , initialized_(true), loaded_(false) 
#endif
      {}

    void LoadedBinary() {
#ifdef DEBUG
      assert(initialized_);
      assert(!loaded_);
      loaded_ = true;
#endif
      // Restore the size.  
      end_ = begin_ + *size_ptr_;
    }

    // Caller responsible for not exceeding specified size.  Do not call after FinishedInserting.  
    template <class T> void Insert(const T &t) {
#ifdef DEBUG
      assert(initialized_);
      assert(!loaded_);
#endif
      *end_ = t;
      ++end_;
    }

    void FinishedInserting() {
#ifdef DEBUG
      assert(initialized_);
      assert(!loaded_);
      loaded_ = true;
#endif
      std::sort(begin_, end_);
      *size_ptr_ = (end_ - begin_);
    }

    // Do not call before FinishedInserting.  
    template <class Key> bool Find(const Key key, ConstIterator &out) const {
#ifdef DEBUG
      assert(initialized_);
      assert(loaded_);
#endif
      return SortedUniformFind<ConstIterator, Key>(ConstIterator(begin_), ConstIterator(end_), key, out);
    }

    ConstIterator begin() const { return begin_; }
    ConstIterator end() const { return end_; }

  private:
    typename Packing::MutableIterator begin_, end_;
    uint64_t *size_ptr_;
#ifdef DEBUG
    bool initialized_;
    bool loaded_;
#endif
};

} // namespace util

#endif // UTIL_SORTED_UNIFORM__

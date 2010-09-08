#ifndef UTIL_SORTED_UNIFORM__
#define UTIL_SORTED_UNIFORM__

#include <algorithm>
#include <cstddef>
#include <functional>

#include <inttypes.h>

namespace util {

inline std::size_t Pivot(uint64_t off, uint64_t range, std::size_t width) {
  std::size_t ret = static_cast<std::size_t>(static_cast<float>(off) / static_cast<float>(range) * static_cast<float>(width));
  // Cap for floating point rounding
  return (ret < width) ? ret : width - 1;
}
inline std::size_t Pivot(uint32_t off, uint32_t range, std::size_t width) {
  return static_cast<std::size_t>(static_cast<uint64_t>(off) * static_cast<uint64_t>(width) / static_cast<uint64_t>(range));
}
inline std::size_t Pivot(uint16_t off, uint16_t range, std::size_t width) {
  return static_cast<std::size_t>(static_cast<std::size_t>(off) * width / static_cast<std::size_t>(range));
}
inline std::size_t Pivot(uint8_t off, uint8_t range, std::size_t width) {
  return static_cast<std::size_t>(static_cast<std::size_t>(off) * width / static_cast<std::size_t>(range));
}

// For consistent API with ProbingSearch.
struct SortedUniformInit {};

// Define a Pivot function to match Key.  
template <class KeyT, class ValueT> class SortedUniformMap {
  public:
    typedef KeyT Key;
    typedef ValueT Value;
    typedef SortedUniformInit Init;

    static std::size_t Size(Init ignore, std::size_t entries) {
      return entries * sizeof(Entry);
    }

    SortedUniformMap() {}

    SortedUniformMap(Init ignore, char *start, std::size_t entries) : begin_(reinterpret_cast<Entry*>(start)), end_(begin_) {}

    // Caller responsible for not exceeding specified size.  Do not call after FinishedInserting.  
    void Insert(const Key &key, const Value &value) {
      end_->key = key;
      end_->value = value;
      ++end_;
    }

    void FinishedInserting() {
      std::sort(begin_, end_, LessEntry());
    }

    // Do not call before FinishedInserting.  
    bool Find(const Key &key, const Value *&value) const {
      const Entry *begin = begin_;
      const Entry *end = end_;
      while (begin != end) {
        if (key <= begin->key) {
          if (key != begin->key) return false;
          value = &begin->value;
          return true;
        }
        if (key >= (end - 1)->key) {
          if (key != (end - 1)->key) return false;
          value = &(end - 1)->value;
          return true;
        }
        Key off = key - begin->key;
        const Entry *pivot = begin + Pivot(off, (end - 1)->key - begin->key, end - begin);
        if (pivot->key > key) {
          end = pivot;
        } else if (pivot->key < key) {
          begin = pivot + 1;
        } else {
          value = &pivot->value;
          return true;
        }
      }
      return false;
    }

  private:
    struct Entry {
      Key key;
      Value value;
    };

    struct LessEntry : public std::binary_function<const Entry &, const Entry &, bool> {
      bool operator()(const Entry &left, const Entry &right) const {
        return left.key < right.key;
      }
    };

    Entry *begin_;
    Entry *end_;
};

} // namespace util

#endif // UTIL_SORTED_UNIFORM__

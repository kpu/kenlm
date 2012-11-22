#include <tpie/progress_indicator_null.h>
#include <tpie/sort.h>
#include <tpie/file_stream.h>

#include <functional>

namespace lm {
namespace builder {

struct SuffixOrder {
  template <class Gram> struct Comparator : public std::binary_function<const Gram &, const Gram &, bool> {
    inline bool operator()(const Gram& lhs, const Gram& rhs) const {
      for (int i = Gram::n - 1; i >= 0; --i) {
        if (rhs.w[i] < lhs.w[i]) {
          return false;
        } else if (rhs.w[i] > lhs.w[i]) {
          return true;
        }
      }
      return false;
    }
  };
};

struct ContextOrder {
  template <class Gram> struct Comparator : public std::binary_function<const Gram &, const Gram &, bool> {
    inline bool operator()(const Gram &lhs, const Gram &rhs) const {
      for (int i = Gram::n - 2; i >= 0; --i) {
        if (rhs.w[i] < lhs.w[i]) {
          return false;
        } else if (rhs.w[i] > lhs.w[i]) {
          return true;
        }
      }
      return rhs.w[Gram::n - 1] > lhs.w[Gram::n - 1];
    }
  };
};

template <class Gram, class Compare> void Sort(const char* filename) {
  tpie::file_stream<Gram> stream;
  tpie::progress_indicator_null indicator;
  stream.open(filename);
  typename Compare::template Comparator<Gram> compare;
  tpie::sort(stream, compare, indicator);
}

} // namespace builder
} // namespace lm


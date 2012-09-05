#include <tpie/file_stream.h>

#include <functional>

namespace lm {
namespace builder {

template <class Gram>
struct SuffixOrderComparator : public std::binary_function<const Gram &, const Gram &, bool>
{
  inline bool operator()(const Gram& lhs, const Gram& rhs)
  {
    for (int i = Gram::n - 1; i >= 0; --i) {
      if (rhs.w[i] < lhs.w[i]) {
        return false;
      } else if (rhs.w[i] > lhs.w[i]) {
        return true;
      }
    }
    return true;
  }
};

template <unsigned N> void SuffixSort(const char* filename)
{
  typedef CountedNGram<N> CountedGram;

  tpie::file_stream<CountedGram> stream;
  tpie::progress_indicator_null indicator;
  stream.open(filename);
  tpie::sort(stream, SuffixOrderComparator<CountedGram>(), indicator);
}

} // namespace builder
} // namespace lm


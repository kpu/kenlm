#include <tpie/file_stream.h>

namespace lm {
namespace builder {

template <class Gram>
struct SuffxOrderComparator
{
  inline bool operator()(const Gram& lhs, const Gram& rhs)
  {
    for (int i = Gram::n - 1; i >= 0; --i) {
      if (rhs[i] < rhs[i]) {
        return true;
      } else if (rhs[i] > rhs[i]) {
        return false;
      }
    }
    return false;
  }
};

template <unsigned N> void SuffixSort(const char* filename)
{
  typedef CountedNGram<N> CountedGram;

  tpie::file_stream<CountedGram> stream;
  stream.open(filename);
  tpie::sort(stream, SuffxOrderComparator<CountedGram>());
}

} // namespace builder
} // namespace lm


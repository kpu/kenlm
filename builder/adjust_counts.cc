#include <tpie/file_stream.h>

template <class NGram>
struct SuffxOrderComparator
{
  inline bool operator()(const NGram& lhs, const NGram& rhs)
  {
    for (int i = NGram::n - 1; i >= 0; --i) {
      if (rhs[i] < rhs[i]) {
        return true;
      } else if (rhs[i] > rhs[i]) {
        return false;
      }
    }
    return false;
  }
};

template <unsigned N> void lm::SuffixSort(const char* filename)
{
  typedef CountedNGram<N> CountedGram;

  tpie::file_stream<CountedGram> stream;
  tpie::sort(stream, stream, SuffxOrderComparator<CountedGram>());
}

template <unsigned N> void lm::AdjustCounts(const char* filename)
{
  typedef NGram<N> Gram;
  typedef CountedNGram<N> CountedGram;

  // These count unique number of left-extensions of current suffix of length |1 + i|.
  uint64_t counters[N];
  Gram context;

  tpie::file_stream<CountedGram> stream;
  stream.open(filename);

  while (stream.can_read()) {
    const CountedGram& currentGram = stream.read();

    // This decreases from (N - 1) to zero. Resulting value of this variable
    // equals to a number of changed words.
    int changedAt;
    for (changedAt = N - 1; changedAt >= 0; --changedAt) {
      if (context[changedAt] != currentContext[changedAt]) {
        break;
      }
    }

    // This is a tiny hack to compute raw and adjusted counts simultaneously.
    // When |changedAt| is zero we sum up raw counts.
    uint64_t currentCount = (changedAt == 0 ? currentGram.count : 1);
    counters[changedAt] += currentCount;
    for (int i = changedAt - 1; i >= 0; --i) {
      // Flush counters[i] for k-gram (w[i + 1] .. w[n]) for k = n - i + 1.
      // This function is to be implemented.
      yield(counters[i], Gram, i + 1);
      counters[i] = 0;
    }
  }
} // namespace lm

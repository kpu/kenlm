#include <tpie/file_stream.h>

namespace lm {
namespace builder {

template <unsigned N> void AdjustCounts(const char* filename)
{
  typedef NGram<N> Gram;
  typedef CountedNGram<N> CountedGram;

  // These count unique number of left-extensions of current suffix of length |1 + i|.
  uint64_t counters[N];
  Gram gram;

  tpie::file_stream<CountedGram> stream;
  stream.open(filename);

  while (stream.can_read()) {
    const CountedGram& currentGram = stream.read();

    // XXX(sandello): Remove me, please.
    for (unsigned i = 0; i < N; ++i) {
      std::cout << currentGram.w[i] << "\t";
    }
    std::cout << currentGram.count << std::endl;

    // This decreases from (N - 1) to zero. Resulting value of this variable
    // equals to a number of changed words.
    int changedAt;
    for (changedAt = N - 1; changedAt >= 0; --changedAt) {
      if (gram.w[changedAt] != currentGram.w[changedAt]) {
        break;
      }
    }

    // XXX(sandello): Remove me, please.
    std::cout << "Changed at: " << changedAt << std::endl;

    // This is a tiny hack to compute raw and adjusted counts simultaneously.
    // When |changedAt| is zero we sum up raw counts.
    uint64_t currentCount = (changedAt == 0 ? currentGram.count : 1);
    counters[changedAt] += currentCount;
    for (int i = changedAt - 1; i >= 0; --i) {
      // Flush counters[i] for k-gram (w[i + 1] .. w[n]) for k = n - i + 1.
      // This function is to be implemented.
      //yield(counters[i], Gram, i + 1);
      counters[i] = 0;
    }
  }
}

} // namespace builder
} // namespace lm


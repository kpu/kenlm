#include "builder/ngram.hh"
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
  stream.open(filename, tpie::access_read);

  {
    const CountedGram& firstGram = stream.read();
    ::memcpy(&gram.w, &firstGram.w, sizeof(gram.w));
    for (unsigned i = 0; i < N; ++i) {
      counters[i] = (i == 0 ? firstGram.count : 1);
    }
  }

  while (stream.can_read()) {
    const CountedGram& currentGram = stream.read();

    // Resulting value of this variable equals to the number of changed words.
    int changedAt;
    for (changedAt = N; changedAt > 0; --changedAt) {
      if (gram.w[changedAt - 1] != currentGram.w[changedAt - 1]) {
        break;
      }
    }

    // XXX(sandello): Remove me, please.
    for (unsigned i = 0; i < N; ++i) {
      std::cout << gram.w[i] << "\t";
    }
    std::cout << counters[0] << "\t|\t";
    for (unsigned i = 0; i < N; ++i) {
      std::cout << currentGram.w[i] << "\t";
    }
    std::cout << currentGram.count << "\t|\t";
    std::cout << "@ " << changedAt << std::endl;
    // XXX(sandello): Till here.

    // changedAt could equal to N; in that case counters are irrelevant.
    if (changedAt < (int)N) {
      counters[changedAt] += (changedAt == 0 ? currentGram.count : 1);
    }

    for (int i = changedAt - 1; i >= 0; --i) {
      // Flush counters[i] for k-gram (w[i] .. w[n]) for k = n - i + 1.
      // yield(counters[i], Gram, i + 1);
      std::cout << "Emitting n-gram: ";
      for (unsigned k = i; k < N; ++k) { std::cout << gram.w[k] << " "; }
      std::cout << " ; Count = " << counters[i] << std::endl;
    }
    for (int i = changedAt - 1; i >= 0; --i) {
      // Reset the appropriate words and counters.
      gram.w[i] = currentGram.w[i];
      counters[i] = (i == 0 ? currentGram.count : 1);
    }
  }
}

} // namespace builder
} // namespace lm


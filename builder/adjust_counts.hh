#ifndef BUILDER_ADJUST_COUNTS__
#define BUILDER_ADJUST_COUNTS__

#include "builder/ngram.hh"
#include "builder/multi_file_stream.hh"

#include <tpie/file_stream.h>

namespace lm {
namespace builder {

template <class MFS> void EmitNGram(MFS& mfs, WordIndex* w, unsigned n, uint64_t count)
{
  std::cout << "Emitting n-gram: ";
  for (unsigned i = 0; i < n; ++i) { std::cout << w[i] << " "; }
  std::cout << " ; Count = " << count << std::endl;

#define XX(i) \
  CountedNGram<i> gram; \
  ::memcpy(&gram.w, w, sizeof(gram.w)); \
  gram.count = count; \
  mfs.template get<i>()->write(gram);

  STATICALLY_DISPATCH(n);
#undef XX
}

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

  const char* files[] = {
    "f1",
    "f2",
    "f3",
    "f4",
    "f5"
    };
  MultiFileStream<N, CountedNGram> mfs(files);

  while (stream.can_read()) {
    const CountedGram& currentGram = stream.read();
    bool shortened = false;

    // Resulting value of this variable equals to the number of changed words.
    int changedAt;
    for (changedAt = N; changedAt > 0; --changedAt) {
      if (gram.w[changedAt - 1] != currentGram.w[changedAt - 1]) {
        break;
      }
    }

    // Check that we have BOS somewhere.
    for (unsigned i = 0; i < N; ++i) {
      if (gram.w[i] == kBOS) {
        shortened = true;
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
      counters[changedAt] += (shortened || changedAt == 0 ? currentGram.count : 1);
    }

    for (int i = changedAt - 1, encounteredBOS = false; i >= 0 && !encounteredBOS; --i) {
      EmitNGram(mfs, gram.w + i, N - i, counters[i]);
      encounteredBOS = (gram.w[i] == kBOS);
    }
    for (int i = changedAt - 1; i >= 0; --i) {
      // Reset the appropriate words and counters.
      gram.w[i] = currentGram.w[i];
      // TODO(sandello): This is obviously wrong. Fallback to raw counts only if i-th word is BOS.
      // TODO(sandello): Think about it.
      counters[i] = (shortened || i == 0 ? currentGram.count : 1);
    }
  }

  for (int i = N - 1, encounteredBOS = false; i >= 0 && !encounteredBOS; --i) {
    EmitNGram(mfs, gram.w + i, N - i, counters[i]);
    encounteredBOS = (gram.w[i] == kBOS);
  }
}

} // namespace builder
} // namespace lm

#endif // BUILDER_ADJUST_COUNTS__


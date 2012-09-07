#include "builder/adjust_counts.hh"
#include "builder/corpus_count.hh"
#include "builder/ngram.hh"
#include "builder/sort.hh"

#include <tpie/tpie.h>
#include <tpie/file_stream.h>
#include <tpie/sort.h>

using namespace lm;
using namespace lm::builder;

// |Print|()s are family of overloaded functions which are called by |DumpFileStream|.
template <unsigned N>
void Print(const CountedNGram<N>& gram)
{
  for (unsigned i = 0; i < N; ++i) {
    std::cout << gram.w[i] << "\t";
  }
  std::cout << gram.count << std::endl;
}

template <class Gram>
void DumpFileStream(const char* filename)
{
  tpie::file_stream<Gram> fs;
  fs.open(filename, tpie::access_read);

  while (fs.can_read()) {
    const Gram& gram = fs.read();
    Print(gram);
  }
}

// |Fill|()s are family of overloaded functions which are called by |GenerateFileStream|.
static const int kNumberOfRecords = 1000;
static const int kWordIdRange = 10;
static const int kCountRange = 30;

template <unsigned N>
void Fill(CountedNGram<N>* gram)
{
  for (unsigned i = 0; i < N; ++i) {
    gram->w[i] = 1 + rand() % kWordIdRange;
  }
  gram->count = rand() % kCountRange;
}

template <class Gram>
void GenerateFileStream(const char* filename)
{
  tpie::file_stream<Gram> fs;
  fs.open(filename, tpie::access_write);

  Gram gram;
  for (int i = 0; i < kNumberOfRecords; ++i) {
    Fill(&gram);
    fs.write(gram);
  }
}

int main(int argc, char** argv)
{
  if (argc < 4) {
    std::cerr << "Usage: " << argv[0] << " dump|gen|sort|adjust <n-gram-order> <filename>" << std::endl;
    return 1;
  }

  const char* routine = argv[1];
  int order = atoi(argv[2]);
  const char* filename = argv[3];

  tpie::tpie_init();
  /****/ if (strcmp(routine, "dump") == 0) {
#define XX(i) DumpFileStream< CountedNGram< i > >(filename)
    STATICALLY_DISPATCH(order);
#undef XX
  } else if (strcmp(routine, "gen") == 0) {
#define XX(i) GenerateFileStream< CountedNGram< i > >(filename)
    STATICALLY_DISPATCH(order);
#undef XX
  } else if (!strcmp(routine, "count")) {
#define XX(i) CorpusCount< i >(filename)
    STATICALLY_DISPATCH(order);
#undef XX
  } else if (strcmp(routine, "sort") == 0) {
#define XX(i) Sort<CountedNGram< i >, SuffixOrder>(filename)
    STATICALLY_DISPATCH(order);
#undef XX
  } else if (strcmp(routine, "adjust") == 0) {
#define XX(i) AdjustCounts< i >(filename)
    STATICALLY_DISPATCH(order);
#undef XX
  } else {
    std::cerr << "Unknown command '" << routine << "'" << std::endl;
  }
  tpie::tpie_finish();

  return 0;
}


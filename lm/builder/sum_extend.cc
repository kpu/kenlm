#include "lm/builder/sum_extend.hh"

#include "lm/builder/ngram_stream.hh"
#include "util/stream/stream.hh"
#include "util/stream/chain.hh"

#include <vector>

namespace lm { namespace builder {

void SumExtend(const util::stream::ChainPosition &input, const util::stream::ChainPosition &output) {
  NGramStream in(input);
  util::stream::Stream out(output);

  std::vector<WordIndex> previous(in->Order() - 1);
  const std::size_t size = sizeof(WordIndex) * previous.size();
  for(; in; ++out) {
    memcpy(&previous[0], in->begin(), size);
    uint64_t &count = *reinterpret_cast<uint64_t*>(out.Get());
    for (count = 1, ++in; in && !memcmp(&previous[0], in->begin(), size); ++in, ++count) {}
  }
  out.Poison();
}

}} // namespaces

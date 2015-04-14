#include "lm/builder/renumber.hh"

#include "lm/builder/ngram_stream.hh"
#include "util/stream/chain.hh"

namespace lm { namespace builder {

void Renumber::Run(const util::stream::ChainPosition &position) {
  for (NGramStream stream(position); stream; ++stream) {
    for (WordIndex *w = stream->begin(); w != stream->end(); ++w) {
      *w = new_numbers_[*w];
    }
  }
}

}} // namespaces

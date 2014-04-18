#include "lm/interpolate/arpa_to_stream.hh"

// TODO: should this move out of builder?
#include "lm/builder/ngram_stream.hh"
#include "lm/read_arpa.hh"
#include "lm/vocab.hh"

namespace lm { namespace interpolate {

ARPAToStream::ARPAToStream(int fd, ngram::GrowableVocab &vocab)
  : in_(fd), vocab_(vocab) {
    
  // Read the ARPA file header.
  //
  // After the following call, counts_ will be correctly initialized,
  // and in_ will be positioned for reading the body of the ARPA file.  
  ReadARPACounts(in_, counts_);
  
}

void ARPAToStream::Run(const util::stream::ChainPositions &positions) {
  
  // TODO: Explain what this call does
  builder::NGramStreams streams(positions);
  PositiveProbWarn warn;

  // TODO: Explain why unigrams are handled separately from higher order n-grams
  // Unigrams.
  ReadNGramHeader(in_, 1);
  for (uint64_t i = 0; i < counts_[0]; ++i, ++streams[0]) {
    streams[0]->begin()[0] = vocab_.FindOrInsert(Read1Gram(in_, streams[0]->Value().complete, warn));
  }
  // TODO: Explain what this call does
  streams[0].Poison();

  // TODO: don't waste backoff field for highest order.
  for (unsigned char n = 2; n <= counts_.size(); ++n) {
    ReadNGramHeader(in_, n);
    builder::NGramStream &stream = streams[n - 1];
    const uint64_t end = counts_[n - 1];
    for (std::size_t i = 0; i < end; ++i, ++stream) {
      ReadNGram(in_, n, vocab_, stream->begin(), stream->Value().complete, warn);
    }
    // TODO: Explain what this call does
    stream.Poison();
  }
}

}} // namespaces

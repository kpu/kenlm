#include "lm/interpolate/arpa_to_stream.hh"

// TODO: should this move out of builder?
#include "lm/builder/ngram_stream.hh"
#include "lm/read_arpa.hh"
#include "lm/vocab.hh"

namespace lm { namespace interpolate {

ARPAToStream::ARPAToStream(int fd, ngram::GrowableVocab<ngram::WriteUniqueWords> &vocab)
  : in_(fd), vocab_(vocab) {
    
  // Read the ARPA file header.
  //
  // After the following call, counts_ will be correctly initialized,
  // and in_ will be positioned for reading the body of the ARPA file.  
  ReadARPACounts(in_, counts_);
  
}

void ARPAToStream::Run(const util::stream::ChainPositions &positions) {
  // Make one stream for each order.
  builder::NGramStreams streams(positions);
  PositiveProbWarn warn;

  // Unigrams are handled specially because they're being inserted into the vocab.
  ReadNGramHeader(in_, 1);
  for (uint64_t i = 0; i < counts_[0]; ++i, ++streams[0]) {
    streams[0]->begin()[0] = vocab_.FindOrInsert(Read1Gram(in_, streams[0]->Value().complete, warn));
  }
  // Finish off the unigram stream.
  streams[0].Poison();

  // TODO: don't waste backoff field for highest order.
  for (unsigned char n = 2; n <= counts_.size(); ++n) {
    ReadNGramHeader(in_, n);
    builder::NGramStream &stream = streams[n - 1];
    const uint64_t end = counts_[n - 1];
    for (std::size_t i = 0; i < end; ++i, ++stream) {
      ReadNGram(in_, n, vocab_, stream->begin(), stream->Value().complete, warn);
    }
    // Finish the stream for n-grams..
    stream.Poison();
  }
}

}} // namespaces

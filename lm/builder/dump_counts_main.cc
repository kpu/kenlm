#include "lm/builder/print.hh"
#include "util/file.hh"
#include "util/stream/chain.hh"
#include "util/stream/io.hh"

#include <iostream>

#include <boost/lexical_cast.hpp>

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cerr << "Expected null-delimited vocab file and order." << std::endl;
    return 1;
  }
  util::scoped_fd vocab(util::OpenReadOrThrow(argv[1]));
  lm::builder::VocabReconstitute reconstitute(vocab.get());
  util::stream::Chain(util::stream::ChainConfig(boost::lexical_cast<unsigned int>(argv[2])*4+8, 1, 4096)) >> util::stream::Read(0) >> lm::builder::Print<uint64_t>(reconstitute, std::cout);
}

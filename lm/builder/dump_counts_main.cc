#include "lm/builder/print.hh"
#include "util/file.hh"
#include "util/stream/chain.hh"
#include "util/stream/io.hh"
#include <iostream>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Expected null-delimited vocab file." << std::endl;
    return 1;
  }
  util::scoped_fd vocab(util::OpenReadOrThrow(argv[1]));
  lm::builder::VocabReconstitute reconstitute(vocab.get());
  util::stream::Chain(util::stream::ChainConfig(5*4+8, 1, 4096)) >> util::stream::Read(0) >> lm::builder::Print<uint64_t>(reconstitute, std::cout);
}

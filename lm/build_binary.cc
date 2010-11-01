#include "lm/model.hh"

#include <iostream>

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " input.arpa output.mmap" << std::endl;
    return 1;
  }
  lm::ngram::Config config;
  config.write_mmap = argv[2];
  lm::ngram::Model(argv[1], config);
}

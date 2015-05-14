
#include "lm/interpolate/enumerate_global_vocab.hh"

#include <iostream>

namespace lm {
//constructor

  EnumerateGlobalVocab::EnumerateGlobalVocab(int foo) {
    
    std::cerr << "Vocab Builder: " <<  foo << std::endl;
  }

  void EnumerateGlobalVocab::Add(WordIndex index, const StringPiece &str) {

    //test
    std::cerr << "Vocab add: " << str << " from " << index << std::endl;
    
  }

}

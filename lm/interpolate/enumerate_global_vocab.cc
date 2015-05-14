
#include "lm/interpolate/enumerate_global_vocab.hh"

#include <iostream>
#include <map>

namespace lm {
//constructor
  
  EnumerateGlobalVocab::EnumerateGlobalVocab(std::map<StringPiece, int*> * vm, int nm) {

    vmap = vm;
    num_models = nm;
    cur_model = 0; //blah
    
    std::cerr << "Vocab Builder with models: " <<  nm << std::endl;
  }

  void EnumerateGlobalVocab::Add(WordIndex index, const StringPiece &str) {

    //check for existence of key
    std::map<StringPiece, int*>::iterator itr = vmap->find(str);

    //put stuff
    if(itr != vmap->end()) {
      std::cerr << "Vocab exist: " << str << " M: " << cur_model << " I:" << index << std::endl;
      itr->second[cur_model] = index;
    }
    //new key
    else {
      std::cerr << "Vocab add: " << str << " M: " << cur_model << " I:" << index << std::endl;
      
      //create model index map for this vocab word
      //init to 0, 0 is UNK
      int * indices = new int[num_models];
      memset(indices, 0, (sizeof(int)*num_models)); //this still legit?

      indices[cur_model] = index;
      (*vmap)[str] = indices;
    }

    
  }

}

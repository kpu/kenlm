
#include "lm/interpolate/enumerate_global_vocab.hh"

#include <iostream>
#include <map>

namespace lm {
//constructor

  EnumerateGlobalVocab::EnumerateGlobalVocab(std::map<std::string, int*> * vm, int nm) {

    vmap = vm;
    num_models = nm;
    cur_model = 0; //blah
    cnt = 0;
    std::cerr << "Vocab Builder with models: " <<  nm << std::endl;
  }

  void EnumerateGlobalVocab::Add(WordIndex index, const StringPiece &str) {

    std::string st = str.as_string();

    //check for existence of key
    std::map<std::string, int*>::iterator itr = vmap->find(st);

    //put stuff
    if(itr != vmap->end()) {
      std::cerr << "Vocab exist: " << str << " M: " << cur_model << " I:" << index << std::endl;
      itr->second[cur_model] = index;
    }
    //new key
    else {

      //create model index map for this vocab word
      //init to 0, 0 is UNK
      int * indices = new int[num_models];
      memset(indices, 0, (sizeof(int)*num_models)); //this still legit?

      indices[cur_model] = index;
      (*vmap)[st] = indices;
      std::cerr << cnt << ":Vocab add: " << str << " M: " << cur_model << " I:" << index << std::endl;
      cnt++;
    }


  }

}

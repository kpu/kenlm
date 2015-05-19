#ifndef LM_ENUMERATE_GLOBAL_VOCAB_H
#define LM_ENUMERATE_GLOBAL_VOCAB_H

#include "lm/enumerate_vocab.hh"
#include <map>

/* Use this to create a global vocab across models for use when
 * calculating lambdas for interpolation. Or other stuff.
 */
namespace lm {

  class EnumerateGlobalVocab : EnumerateVocab {

  public:

    //yes, ugly...
    std::map<std::string, int*> * vmap;
    int num_models;
    int cur_model;
    int cnt; //stupid

    ~EnumerateGlobalVocab() {}

    void Add(WordIndex index, const StringPiece & str);

    EnumerateGlobalVocab(std::map<std::string, int*> *, int);

    void SetCurModel(int i) { cur_model = i; }

  protected:
    EnumerateGlobalVocab() {}

  };

} //namespace lm

#endif // LM_ENUMERATE_GLOBAL_VOCAB_H


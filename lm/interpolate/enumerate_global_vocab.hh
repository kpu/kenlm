#ifndef LM_ENUMERATE_GLOBAL_VOCAB_H
#define LM_ENUMERATE_GLOBAL_VOCAB_H

#include "lm/enumerate_vocab.hh"


/* Use this to create a global vocab across models for use when
 * calculating lambdas for interpolation. Or other stuff.
 */
namespace lm {

  class EnumerateGlobalVocab : EnumerateVocab {
    
  public:

    ~EnumerateGlobalVocab();

    void Add(WordIndex index, const StringPiece & str);

  protected:
    EnumerateGlobalVocab();
   

  };

} //namespace lm

#endif // LM_ENUMERATE_VOCAB_H


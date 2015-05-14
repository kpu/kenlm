#include "lm/interpolate/merge_vocab.hh"
#include "lm/interpolate/universal_vocab.hh"

#include <stdio.h>
#include <iostream>

int main()
{
  
  std::vector<lm::interpolate::ModelInfo> vocab_info;
  
  FILE *fp1, *fp2, *fp3;
  
  if ((fp1 = fopen("test1", "r")) == NULL)
    return -1;
  if ((fp2 = fopen("test2", "r")) == NULL)
    return -1;
  if ((fp3 = fopen("test3", "r")) == NULL)
    return -1;
  
  lm::interpolate::ModelInfo m1, m2, m3;
  m1.fd = fileno(fp1);
  m1.vocab_size = 10;
  m2.fd = fileno(fp2);
  m2.vocab_size = 10;
  m3.fd = fileno(fp3);
  m3.vocab_size = 10;

  
  vocab_info.push_back(m1);
  vocab_info.push_back(m2);
  vocab_info.push_back(m3);
  
  std::vector<lm::WordIndex> model_max_idx;
  model_max_idx.push_back(m1.vocab_size);
  model_max_idx.push_back(m2.vocab_size);
  model_max_idx.push_back(m3.vocab_size);
  
  lm::interpolate::UniversalVocab universal_vocab(model_max_idx);
  lm::interpolate::MergeVocabIndex merger(vocab_info, universal_vocab);
  
  std::cout << universal_vocab.GetUniversalIdx(0, 0) << std::endl;
  std::cout << universal_vocab.GetUniversalIdx(1, 0) << std::endl;
  std::cout << universal_vocab.GetUniversalIdx(2, 0) << std::endl;
  
  std::cout << universal_vocab.GetUniversalIdx(0, 1) << std::endl;
  std::cout << universal_vocab.GetUniversalIdx(1, 1) << std::endl;
  std::cout << universal_vocab.GetUniversalIdx(2, 1) << std::endl;
  
  std::cout << universal_vocab.GetUniversalIdx(0, 5) << std::endl;
  std::cout << universal_vocab.GetUniversalIdx(1, 3) << std::endl;
  std::cout << universal_vocab.GetUniversalIdx(2, 3) << std::endl;

  return 0;
}

/* Faster code path for building binary format files for querying. */
#ifndef LM_BUILDER_BINARIZE__
#define LM_BUILDER_BINARIZE__

#include "lm/config.hh"
#include "lm/model.hh"
#include "lm/vocab.hh"
#include "lm/word_index.hh"

#include <vector>

#include <stdint.h>

namespace lm {
namespace builder {
class ChainPositions;

class Binarize {
  private:
    typedef ngram::QuantArrayTrieModel Model;
  public:
    Binarize(const std::vector<uint64_t> &counts, const ngram::Config &config, int vocab_file, std::vector<WordIndex> &mapping);

    ngram::SeparatelyQuantize &Quantizer() { return model_.search_.quant_; }

    void Run(const ChainPositions &positions);

  private:
    Model model_;
    WordIndex end_sentence_;
    const std::vector<uint64_t> counts_;
    const ngram::Config config_;
    std::size_t vocab_size_;
};
}} // namespaces

#endif // LM_BUILDER_BINARIZE__

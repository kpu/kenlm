/* Faster code path for building binary format files for querying. */
#ifndef LM_BUILDER_BINARIZE__
#define LM_BUILDER_BINARIZE__

#include "lm/builder/ngram.hh"
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
    Binarize(uint64_t unigram_count, uint8_t order, const ngram::Config &config, int vocab_file, std::vector<WordIndex> &mapping);

    WordIndex EndSentence() const { return end_sentence_; }

    void SetupSearch(const std::vector<uint64_t> &counts);

    typename Model::SearchBackend::Quantizer &Quantizer() { return model_.search_.quant_; }

    void Enter(unsigned int, NGram &gram) {
      model_.search_.ExternalInsert(gram.Order(), *gram.begin(), gram.Value().complete);
    }

    void Run(const ChainPositions &positions);

    void Finish();

  private:
    Model model_;
    WordIndex end_sentence_;
    std::vector<uint64_t> counts_;
    const ngram::Config config_;
    std::size_t vocab_size_;
};
}} // namespaces

#endif // LM_BUILDER_BINARIZE__

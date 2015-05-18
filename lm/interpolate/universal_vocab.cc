#include "lm/interpolate/universal_vocab.hh"

namespace lm {
namespace interpolate {

UniversalVocab::UniversalVocab(const std::vector<WordIndex>& model_max_idx) {
  model_index_map_.resize(model_max_idx.size());
  for (size_t i = 0; i < model_max_idx.size(); ++i) {
    model_index_map_[i].resize(model_max_idx[i]);
  }
}

}} // namespaces

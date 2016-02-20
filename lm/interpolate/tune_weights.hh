#ifndef LM_INTERPOLATE_TUNE_WEIGHTS_H
#define LM_INTERPOLATE_TUNE_WEIGHTS_H

#include "lm/interpolate/tune_matrix.hh"
#include "util/string_piece.hh"

#include <vector>

namespace lm { namespace interpolate {
class InstancesConfig;

// Run a tuning loop, producing weights as output.
void TuneWeights(int tune_file, const std::vector<StringPiece> &model_names, const InstancesConfig &config, Vector &weights);

}} // namespaces
#endif // LM_INTERPOLATE_TUNE_WEIGHTS_H

#ifndef LM_INTERPOLATE_TUNE_DERIVATIVES_H
#define LM_INTERPOLATE_TUNE_DERIVATIVES_H

#include "lm/interpolate/tune_matrix.hh"

#include <Eigen/Core>
#include <cmath>

namespace lm { namespace interpolate {

class Instances;

// Given tuning instances and model weights, computes the objective function (log probability), gradient, and Hessian.
// Returns log probability / number of instances.
void Derivatives(Instances &instances /* Doesn't modify but ReadExtensions is lazy */, Vector &weights);

}} // namespaces

#endif // LM_INTERPOLATE_TUNE_DERIVATIVES_H


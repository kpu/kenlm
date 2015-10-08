#ifndef LM_INTERPOLATE_TUNE_MATRIX_H
#define LM_INTERPOLATE_TUNE_MATRIX_H

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#include <Eigen/Core>
#pragma GCC diagnostic pop

namespace lm { namespace interpolate {
 
typedef Eigen::MatrixXd Matrix;
typedef Eigen::VectorXd Vector;

typedef Matrix::Scalar Accum;

}} // namespaces
#endif // LM_INTERPOLATE_TUNE_MATRIX_H

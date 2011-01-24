#ifndef LM_BLANK__
#define LM_BLANK__
#include <limits>

namespace lm {
namespace ngram {

const float kBlankProb = -std::numeric_limits<float>::infinity();
const float kBlankBackoff = 0.0;

} // namespace ngram
} // namespace lm
#endif // LM_BLANK__

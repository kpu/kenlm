#ifndef LM_BLANK__
#define LM_BLANK__
#include <limits>

namespace lm {
namespace ngram {

const float kBlankProb = -std::numeric_limits<float>::quiet_NaN();
const float kBlankBackoff = std::numeric_limits<float>::quiet_NaN();

} // namespace ngram
} // namespace lm
#endif // LM_BLANK__

// Separate header because this is used often.
#ifndef LM_WORD_INDEX__
#define LM_WORD_INDEX__

#include <limits.h>

namespace lm {
typedef unsigned int WordIndex;
const WordIndex kMaxWordIndex = UINT_MAX;
} // namespace lm

typedef lm::WordIndex LMWordIndex;

#endif

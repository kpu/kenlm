// Separate header because this is used often.
#ifndef LM_WORD_INDEX_H
#define LM_WORD_INDEX_H

#include <limits.h>

namespace lm {
typedef unsigned int WordIndex;
const WordIndex kMaxWordIndex = UINT_MAX;
} // namespace lm

typedef lm::WordIndex LMWordIndex;

#endif

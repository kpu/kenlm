#ifndef BUILDER_DISCOUNT__
#define BUILDER_DISCOUNT__

#include <algorithm>

#include <inttypes.h>

namespace lm {
namespace builder {

struct Discount {
  float amount[4];

  float Apply(uint64_t count) const {
    return static_cast<float>(count) - amount[std::min<uint64_t>(count, 3)];
  }
};

} // namespace builder
} // namespace lm

#endif // BUILDER_DISCOUNT__

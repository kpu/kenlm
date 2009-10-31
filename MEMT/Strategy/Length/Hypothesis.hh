#ifndef _MEMT_Strategy_Length_Hypothesis_h
#define _MEMT_Strategy_Length_Hypothesis_h

namespace strategy {
namespace length {

struct Hypothesis {};

bool operator==(const Hypothesis left, const Hypothesis right) {
  return true;
}

size_t hash_value(const Hypothesis value) {
  // Mashing on keyboard.
  return 415648974;
}

} // namespace length
} // namespace strategy

#endif // _MEMT_Strategy_Length_Hypothesis_h

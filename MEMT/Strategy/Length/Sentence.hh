#ifndef _MEMT_Strategy_Length_Sentence_h
#define _MEMT_Strategy_Length_Sentence_h

#include "MEMT/Strategy/Length/Config.hh"
#include "MEMT/Strategy/Length/Hypothesis.hh"

namespace strategy {
namespace length {

class Sentence {
  public:
    typedef length::Hypothesis Hypothesis;
    typedef length::Config Config;

    Sentence() {}

    void Reset(const Config &config) {}

    size_t BothFeatures() const { return 0; }
    size_t EndFeatures() const { return 1; }

    void Begin(Hypothesis &start_state, LogScore *start_scores) const {}

    void Extend(
        const input::Input &input,
        const decoder::HypHistory *history,
        const input::Location &append,
        const Hypothesis &from_state,
        const LogScore *from_scores,
        Hypothesis &to_state,
        LogScore *to_scores) const {}

    void End(size_t length, LogScore *out) const {
      out->MutableLog() = static_cast<LinearScore>(length);
    }
};

} // namespace length
} // namespace strategy

#endif // _MEMT_Strategy_Length_Sentence_h

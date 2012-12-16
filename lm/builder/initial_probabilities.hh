#ifndef LM_BUILDER_INITIAL_PROBABILITIES__
#define LM_BUILDER_INITIAL_PROBABILITIES__

#include "lm/builder/discount.hh"
#include "util/stream/chain.hh"
#include "util/file.hh"

namespace lm {
namespace builder {

/* Compute initial (uninterpolated) probabilities
 * Input: context sorted adjusted counts.  The file is read twice in
 * near-parallel threads.  
 * Output: context sorted uninterpolated counts.  
 */
class InitialProbabilities {
  public:
    InitialProbabilities(
        int input_file, 
        const util::stream::ChainConfig &adder_in,
        const util::stream::ChainConfig &adder_out, 
        const Discount &discount)
      : input_file_(input_file),
        adder_in_(adder_in), adder_out_(adder_out), 
        discount_(discount) {}

    void Run(const util::stream::ChainPosition &main_chain);

  private:
    int input_file_;
    util::stream::ChainConfig adder_in_;
    util::stream::ChainConfig adder_out_;
    Discount discount_;
};

} // namespace builder
} // namespace lm

#endif // LM_BUILDER_INITIAL_PROBABILITIES__

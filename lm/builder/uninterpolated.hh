#ifndef LM_BUILDER_UNINTERPOLATED__
#define LM_BUILDER_UNINTERPOLATED__

#include "lm/builder/discount.hh"
#include "util/stream/chain.hh"
#include "util/file.hh"

namespace lm {
namespace builder {

class Uninterpolated {
  public:
    Uninterpolated(
        int input_file, 
        const util::stream::ChainConfig &adder_in,
        const util::stream::ChainConfig &adder_out, 
        const Discount &discount)
      : input_file_(input_file),
        adder_in_(adder_in), adder_out_(adder_out), 
        discount_(discount) {}

    int Input() const { return input_file_.get(); }

    void Run(const util::stream::ChainPosition &main_chain);

  private:
    util::scoped_fd input_file_;
    util::stream::ChainConfig adder_in_;
    util::stream::ChainConfig adder_out_;
    Discount discount_;
};

} // namespace builder
} // namespace lm

#endif // LM_BUILDER_UNINTERPOLATED__

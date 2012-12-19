#include "lm/builder/initial_probabilities.hh"

#include "lm/builder/discount.hh"
#include "lm/builder/ngram_stream.hh"
#include "lm/builder/sort.hh"
#include "util/file.hh"
#include "util/stream/chain.hh"
#include "util/stream/io.hh"
#include "util/stream/stream.hh"
#include "util/stream/timer.hh"

#include <boost/format.hpp>

#include <vector>

namespace lm { namespace builder {

namespace {
struct BufferEntry {
  // \sum_w a(c w) for all w.  
  float denominator;
  // Gamma from page 20 of Chen and Goodman.  
  float gamma;
};

class AddRight {
  public:
    AddRight(const Discount &discount, const util::stream::ChainPosition &input) 
      : discount_(discount), input_(input) {}

    void Run(const util::stream::ChainPosition &output) {
      NGramStream in(input_);
      util::stream::Stream out(output);

      std::vector<WordIndex> previous(in->Order() - 1);
      const std::size_t size = sizeof(WordIndex) * previous.size();
      for(; in; ++out) {
        memcpy(&previous[0], in->begin(), size);
        uint64_t denominator = 0;
        uint64_t counts[4];
        memset(counts, 0, sizeof(counts));
        do {
          denominator += in->Count();
          ++counts[std::min(in->Count(), static_cast<uint64_t>(3))];
        } while (++in && !memcmp(&previous[0], in->begin(), size));
        BufferEntry &entry = *reinterpret_cast<BufferEntry*>(out.Get());
        entry.denominator = static_cast<float>(denominator);
        entry.gamma = 0.0;
        for (unsigned i = 1; i <= 3; ++i) {
          entry.gamma += discount_.Get(i) * static_cast<float>(counts[i]);
        }
        entry.gamma /= entry.denominator;
      }
      out.Poison();
    }

  private:
    const Discount &discount_;
    const util::stream::ChainPosition input_;
};

class MergeRight {
  public:
    MergeRight(
        const InitialProbabilitiesConfig &config, const util::stream::ChainPosition &secondary_reader, const Discount &discount)
      : config_(config), secondary_reader_(secondary_reader), discount_(discount) {}

    void Run(const util::stream::ChainPosition &main_chain);

  private:
    InitialProbabilitiesConfig config_;
    util::stream::ChainPosition secondary_reader_;
    Discount discount_;
};

// calculate the initial probability of each n-gram (before order-interpolation)
// Run() gets invoked once for each order
void MergeRight::Run(const util::stream::ChainPosition &main_chain) {
  config_.adder_out.entry_size = sizeof(BufferEntry);

  util::stream::Chain add_out(config_.adder_out);
  add_out >> AddRight(discount_, secondary_reader_);
  util::stream::Stream summed(add_out.Add());
  add_out >> util::stream::kRecycle;

  NGramStream grams(main_chain);
  std::string timer_msg = (boost::format("(%%w s) Calculated initial probabilities for %1%-grams\n") % grams->Order()).str();
  UTIL_TIMER(timer_msg);

  // Without interpolation, the interpolation weight goes to <unk>.
  if (grams->Order() == 1 && !config_.interpolate_unigrams) {
    BufferEntry sums(*static_cast<const BufferEntry*>(summed.Get()));
    assert(*grams->begin() == kUNK);
    grams->Value().uninterp.prob = sums.gamma;
    grams->Value().uninterp.gamma = 0.0;
    while (++grams) {
      grams->Value().uninterp.prob = discount_.Apply(grams->Count()) / sums.denominator;
      grams->Value().uninterp.gamma = 0.0;
    }
    ++summed;
    return;
  }

  std::vector<WordIndex> previous(grams->Order() - 1);
  const std::size_t size = sizeof(WordIndex) * previous.size();
  for (; grams; ++summed) {
    memcpy(&previous[0], grams->begin(), size);
    const BufferEntry &sums = *static_cast<const BufferEntry*>(summed.Get());
    do {
      Payload &pay = grams->Value();
      pay.uninterp.prob = discount_.Apply(pay.count) / sums.denominator;
      pay.uninterp.gamma = sums.gamma;
    } while (++grams && !memcmp(&previous[0], grams->begin(), size));
  }
}

} // namespace

void InitialProbabilities(const InitialProbabilitiesConfig &config, const std::vector<Discount> &discounts, Chains &primary, Chains &secondary) {
  for (size_t i = 0; i < primary.size(); ++i) {
    primary[i] >> MergeRight(config, secondary[i].Add(), discounts[i]);
    secondary[i] >> util::stream::kRecycle;
  }
}

}} // namespaces

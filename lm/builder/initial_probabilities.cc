#include "lm/builder/initial_probabilities.hh"

#include "lm/builder/discount.hh"
#include "lm/builder/ngram_stream.hh"
#include "lm/builder/sort.hh"
#include "util/file.hh"
#include "util/stream/chain.hh"
#include "util/stream/io.hh"
#include "util/stream/stream.hh"

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
        const InitialProbabilitiesConfig &config, int input_file,  const Discount &discount)
      : config_(config), input_file_(input_file), discount_(discount) {}

    void Run(const util::stream::ChainPosition &main_chain);

  private:
    InitialProbabilitiesConfig config_;
    int input_file_;
    Discount discount_;
};

void MergeRight::Run(const util::stream::ChainPosition &main_chain) {
  util::scoped_fd input(input_file_);
  config_.adder_in.entry_size = main_chain.GetChain().EntrySize();
  config_.adder_out.entry_size = sizeof(BufferEntry);

  util::stream::Chain add_in(config_.adder_in);
  add_in >> util::stream::PRead(input.get());
  util::stream::ChainPosition add_in_pos(add_in.Add());
  add_in >> util::stream::kRecycle;

  util::stream::Chain add_out(config_.adder_out);
  add_out >> AddRight(discount_, add_in_pos);
  util::stream::Stream summed(add_out.Add());
  add_out >> util::stream::kRecycle;

  NGramStream grams(main_chain);
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

void InitialProbabilities(const InitialProbabilitiesConfig &config, const std::vector<Discount> &discounts, Sorts<ContextOrder> &in, Chains &out) {
  for (size_t i = 0; i < in.size(); ++i) {
    util::scoped_fd fd(in[i].StealCompleted());
    out[i] >> util::stream::PRead(fd.get());
    out[i] >> MergeRight(config, fd.release(), discounts[i]);
  }
}

}} // namespaces

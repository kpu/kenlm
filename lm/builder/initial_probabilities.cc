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
  // Gamma from page 20 of Chen and Goodman.  
  float gamma;
  // \sum_w a(c w) for all w.  
  float denominator;
};

// Extract an array of gamma from an array of BufferEntry.  
class OnlyGamma {
  public:
    void Run(const util::stream::ChainPosition &position) {
      for (util::stream::Link block_it(position); block_it; ++block_it) {
        float *out = static_cast<float*>(block_it->Get());
        const float *in = out;
        const float *end = static_cast<const float*>(block_it->ValidEnd());
        for (out += 1, in += 2; in < end; out += 1, in += 2) {
          *out = *in;
        }
        block_it->SetValidSize(block_it->ValidSize() / 2);
      }
    }
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
    MergeRight(bool interpolate_unigrams, const util::stream::ChainPosition &from_adder, const Discount &discount)
      : interpolate_unigrams_(interpolate_unigrams), from_adder_(from_adder), discount_(discount) {}

    // calculate the initial probability of each n-gram (before order-interpolation)
    // Run() gets invoked once for each order
    void Run(const util::stream::ChainPosition &primary) {
      util::stream::Stream summed(from_adder_);

      NGramStream grams(primary);

      // Without interpolation, the interpolation weight goes to <unk>.
      if (grams->Order() == 1 && !interpolate_unigrams_) {
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

  private:
    bool interpolate_unigrams_;
    util::stream::ChainPosition from_adder_;
    Discount discount_;
};

} // namespace

void InitialProbabilities(const InitialProbabilitiesConfig &config, const std::vector<Discount> &discounts, Chains &primary, Chains &second_in, Chains &gamma_out) {
  util::stream::ChainConfig gamma_config = config.adder_out;
  gamma_config.entry_size = sizeof(BufferEntry);
  for (size_t i = 0; i < primary.size(); ++i) {
    util::stream::ChainPosition second(second_in[i].Add());
    second_in[i] >> util::stream::kRecycle;
    gamma_out.push_back(gamma_config);
    gamma_out[i] >> AddRight(discounts[i], second);
    primary[i] >> MergeRight(config.interpolate_unigrams, gamma_out[i].Add(), discounts[i]);
    // Don't bother with the OnlyGamma thread for something to discard.  
    if (i) gamma_out[i] >> OnlyGamma();
  }
}

}} // namespaces

#include "lm/builder/initial_probabilities.hh"

#include "lm/builder/discount.hh"
#include "lm/builder/ngram_stream.hh"
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
        std::cerr << "Denominator " << entry.denominator << " gamma " << entry.gamma << std::endl;
      }
      out.Poison();
    }

  private:
    const Discount &discount_;
    const util::stream::ChainPosition input_;
};

} // namespace

void InitialProbabilities::Run(const util::stream::ChainPosition &main_chain) {
  util::scoped_fd input(input_file_);
  adder_in_.entry_size = main_chain.GetChain().EntrySize();
  adder_out_.entry_size = sizeof(BufferEntry);

  util::stream::Chain add_in(adder_in_);
  add_in >> util::stream::PRead(input.get());
  util::stream::ChainPosition add_in_pos(add_in.Add());
  add_in >> util::stream::kRecycle;

  util::stream::Chain add_out(adder_out_);
  add_out >> AddRight(discount_, add_in_pos);
  util::stream::Stream summed(add_out.Add());
  add_out >> util::stream::kRecycle;

  NGramStream grams(main_chain);
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

}} // namespaces

#include "lm/builder/uninterpolated.hh"

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
        BufferEntry &entry = *reinterpret_cast<BufferEntry*>(out.Get());
        uint64_t denominator = 0;
        entry.gamma = 0.0;
        do {
          denominator += in->Count();
          entry.gamma += discount_.Get(in->Count());
        } while (++in && !memcmp(&previous[0], in->begin(), size));
        entry.denominator = static_cast<float>(denominator);
        entry.gamma /= entry.denominator;
      }
      out.Poison();
    }

  private:
    const Discount &discount_;
    const util::stream::ChainPosition input_;
};

} // namespace

void Uninterpolated::Run(const util::stream::ChainPosition &main_chain) {
  adder_in_.entry_size = main_chain.GetChain().EntrySize();
  adder_out_.entry_size = sizeof(BufferEntry);

  util::stream::Chain add_in(adder_in_);
  add_in >> util::stream::PRead(input_file_.get());
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
  input_file_.reset();
}

}} // namespaces

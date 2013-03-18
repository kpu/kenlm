#include "lm/builder/train_quantizer.hh"

#include "lm/blank.hh"
#include "lm/builder/ngram_stream.hh"

#include <math.h>

namespace lm { namespace builder {

namespace {
FloatCount kInvalidFloatCount(INFINITY);
} // namespace

QuantizeCollector::QuantizeCollector(const util::stream::ChainPosition &position) 
  : block_size_(position.GetChain().BlockSize()),
    table_backing_(util::MallocOrThrow(Table::Size(block_size_ / sizeof(TableEntry), 1.5))),
    table_(table_backing_.get(), Table::Size(block_size_ / sizeof(TableEntry), 1.5), &kInvalidFloatCount),
    block_(position),
    count_(0) {
  table_.Clear();
  current_ = static_cast<FloatCount*>(block_->Get());
  end_ = reinterpret_cast<FloatCount*>(static_cast<uint8_t*>(block_->Get()) + block_size_);
}

void QuantizeCollector::Flush() {
  block_->SetValidSize(reinterpret_cast<uint8_t*>(current_) - static_cast<uint8_t*>(block_->Get()));
  if (block_->ValidSize())
    ++block_;
  current_ = static_cast<FloatCount*>(block_->Get());
  end_ = reinterpret_cast<FloatCount*>(static_cast<uint8_t*>(block_->Get()) + block_size_);
}

// (QuantizeCollector::kMemMultiplier + block_count * sizeof(FloatCount)) * entries_per_block = adding_memory
namespace {
std::size_t ChainUsage(std::size_t adding_memory, std::size_t block_count) {
  // Memory usage per number of entries in a block.
  float per_entry = QuantizeCollector::kMemMultiplier + static_cast<float>(block_count * sizeof(FloatCount));
  std::size_t block_entries = static_cast<float>(adding_memory) / per_entry;
  return block_entries * block_count * sizeof(FloatCount);
}
} // namespace

QuantizeTrainer::QuantizeTrainer(const Config &config) 
  : chain_(util::stream::ChainConfig(
        sizeof(FloatCount),
        config.block_count,
        ChainUsage(config.adding_memory, config.block_count))),
    collector_(chain_.Add()),
    sort_(chain_, config.sort) {}

/* Quantize into bins of equal size as described in
 * M. Federico and N. Bertoldi. 2006. How many bits are needed
 * to store probabilities for phrase-based translation? In Proc.
 * of the Workshop on Statistical Machine Translation, pages
 * 94–101, New York City, June. Association for Computa-
 * tional Linguistics.
 */
void QuantizeTrainer::Train(float *centers, std::size_t center_count_in) {
  sort_.Output(chain_);
  util::stream::TypedStream<FloatCount> stream(chain_.Add());
  chain_ >> util::stream::kRecycle;
  uint64_t center_count = static_cast<uint64_t>(center_count_in);
  uint64_t count = collector_.Count();
  while (center_count && count) {
    uint64_t contain = count / center_count;
    uint64_t included = 0;
    double total = 0.0;
    do {
      total += static_cast<double>(stream->number) * static_cast<double>(stream->count);
      included += stream->count;
      ++stream;
    } while (stream && included < contain && stream->count < (contain - included) * 2);
    *(centers++) = static_cast<float>(total / static_cast<double>(included));
    --center_count;
    count -= included;
  }
  for (; center_count; --center_count) {
    *(centers++) = std::numeric_limits<float>::infinity();
  }
}

QuantizeProbBackoff::QuantizeProbBackoff(const QuantizeTrainer::Config &config) :
  prob_(config.HalfAdding()), backoff_(config.HalfAdding()) {}

void QuantizeProbBackoff::Run(const util::stream::ChainPosition &position) {
  for (NGramStream stream(position); stream; ++stream) {
    const ProbBackoff &value = stream->Value().complete;
    prob_.Add(value.prob);
    if (value.backoff != 0.0)
      backoff_.Add(value.backoff);
  }
  prob_.FinishedAdding();
  backoff_.FinishedAdding();
}

void QuantizeProbBackoff::Train(ngram::SeparatelyQuantize::Bins *out) {
  prob_.Train(out[0].Populate(), 1 << out[0].Bits());
  float *backoff_addr = out[1].Populate();
  backoff_addr[ngram::kExtensionQuant] = ngram::kExtensionBackoff;
  backoff_addr[ngram::kNoExtensionQuant] = ngram::kNoExtensionBackoff;
  backoff_.Train(backoff_addr + 2, (1 << out[1].Bits()) - 2);
}

QuantizeProb::QuantizeProb(const QuantizeTrainer::Config &config) :
  prob_(config) {}

void QuantizeProb::Run(const util::stream::ChainPosition &position) {
  for (NGramStream stream(position); stream; ++stream) {
    prob_.Add(stream->Value().complete.prob);
  }
  prob_.FinishedAdding();
}

void QuantizeProb::Train(ngram::SeparatelyQuantize::Bins &out) {
  prob_.Train(out.Populate(), 1 << out.Bits());
}

}} // namespaces

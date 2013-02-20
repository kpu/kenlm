#include "lm/builder/train_quantizer.hh"

#include <math.h>

namespace lm { namespace builder {

namespace {
FloatCount kInvalidFloatCount(NAN);

} // namespace

QuantizeCollector::QuantizeCollector(const util::stream::ChainPosition &position) 
  : block_size_(position.GetChain().BlockSize()),
    table_backing_(util::CallocOrThrow(Table::Size(block_size_ / sizeof(TableEntry), 1.5))),
    table_(table_backing_.get(), Table::Size(block_size_ / sizeof(TableEntry), 1.5), &kInvalidFloatCount),
    current_(NULL), end_(NULL),
    block_(position) {}

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

QuantizeTrainer::QuantizeTrainer(const util::stream::SortConfig &sort, std::size_t adding_memory, std::size_t block_count) 
  : chain_(util::stream::ChainConfig(
        sizeof(FloatCount),
        block_count, 
        ChainUsage(adding_memory, block_count))),
    collector_(chain_.Add()),
    sort_(chain_, sort),
    count_(0) {}

/* Quantize into bins of equal size as described in
 * M. Federico and N. Bertoldi. 2006. How many bits are needed
 * to store probabilities for phrase-based translation? In Proc.
 * of the Workshop on Statistical Machine Translation, pages
 * 94–101, New York City, June. Association for Computa-
 * tional Linguistics.
 */
void QuantizeTrainer::Train(float *centers, std::size_t center_count_in) {
  sort_.Output(chain_, lazy_memory_);
  util::stream::TypedStream<FloatCount> stream(chain_.Add());
  chain_ >> util::stream::kRecycle;
  uint64_t center_count = static_cast<uint64_t>(center_count_in);
  while (center_count && count_) {
    uint64_t contain = count_ / center_count;
    uint64_t included = 0;
    double total = 0.0;
    do {
      total += static_cast<double>(stream->number) * static_cast<double>(stream->count);
      included += stream->count;
      ++stream;
    } while (stream && included < contain && stream->count < (contain - included) * 2);
    *(centers++) = static_cast<float>(total / static_cast<double>(included));
    --center_count;
    count_ -= included;
  }
  for (; center_count; --center_count) {
    *(centers++) = std::numeric_limits<float>::infinity();
  }
}
    
}} // namespaces

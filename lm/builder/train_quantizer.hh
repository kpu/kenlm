// Scalable training for the quantizer.
#ifndef LM_BUILDER_TRAIN_QUANTIZER_H__
#define LM_BUILDER_TRAIN_QUANTIZER_H__

#include "util/probing_hash_table.hh"
#include "util/scoped.hh"
#include "util/stream/chain.hh"
#include "util/stream/sort.hh"

#include <boost/functional/hash/hash.hpp>

#include <functional>

#include <stdint.h>

namespace lm { namespace builder {

// There's a lot of duplication, so it's worthwhile to count the floats.
#pragma pack(push)
#pragma pack(4)
struct FloatCount {
  float number;
  uint64_t count;
  FloatCount() {}
  FloatCount(float number_in) : number(number_in), count(0) {}
};
#pragma pack(pop)

struct CompareFloatCount : public std::binary_function<const void *, const void *, bool> {
  bool operator()(const void *first, const void *second) const {
    return static_cast<const FloatCount*>(first)->number < static_cast<const FloatCount*>(second)->number;
  }
};

struct CombineFloatCount {
  bool operator()(void *first_void, const void *second_void, const CompareFloatCount &) const {
    FloatCount &first = *static_cast<FloatCount*>(first_void);
    const FloatCount &second = *static_cast<const FloatCount*>(second_void);
    if (first.number != second.number) return false;
    first.count += second.count;
    return true;
  }
};

// Deduplicate numbers and write them to a chain.
class QuantizeCollector {
  private:
    struct TableEntry {
      typedef FloatCount *Key;
      FloatCount *GetKey() const { return key; }
      void SetKey(Key to) { key = to; }
      Key key;
    };

    struct Hash : public std::unary_function<const FloatCount *, std::size_t> {
      std::size_t operator()(const FloatCount *entry) const {
        return boost::hash_value(entry->number);
      }
    };

    struct Equals : public std::binary_function<const FloatCount *, const FloatCount *, bool> {
      bool operator()(const FloatCount *first, const FloatCount *second) const {
        return first->number == second->number;
      }
    };

    typedef util::ProbingHashTable<TableEntry, Hash, Equals> Table;

  public:
    static const float kMemMultiplier = sizeof(TableEntry) * 1.5;

    QuantizeCollector(const util::stream::ChainPosition &position);

    void Add(float number) {
      if (current_ == end_) {
        Flush();
        table_.Clear();
      }
      current_->number = number;
      TableEntry entry;
      entry.key = current_;
      Table::MutableIterator found;
      if (table_.FindOrInsert(entry, found)) {
        current_->count = 1;
        ++current_;
      } else {
        ++(found->key->count);
      }
    }

    void Finish() {
      Flush();
      table_backing_.reset();
      block_.Poison();
    }

  private:
    void Flush();

    const std::size_t block_size_;

    util::scoped_malloc table_backing_;
    Table table_;

    FloatCount *current_, *end_;

    util::stream::Link block_;
};

class QuantizeTrainer {
  public:
    QuantizeTrainer(
        const util::stream::SortConfig &sort,
        std::size_t adding_memory, // Amount of memory to use in the Add phase.
        std::size_t block_count);

    void Add(float value) {
      collector_.Add(value);
      ++count_;
    }

    void FinishedAdding() {
      collector_.Finish();
      chain_.Wait(true);
    }

    std::size_t Merge(std::size_t lazy_memory) {
      lazy_memory_ = sort_.Merge(lazy_memory);
      return lazy_memory_;
    }

    void Train(float *centers, std::size_t center_count);

  private:
    util::stream::Chain chain_;

    QuantizeCollector collector_;

    util::stream::Sort<CompareFloatCount, CombineFloatCount> sort_;

    uint64_t count_;

    std::size_t lazy_memory_;
};

}} // namespaces

#endif // LM_BUILDER_TRAIN_QUANTIZER_H__

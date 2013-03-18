// Scalable training for the quantizer.
#ifndef LM_BUILDER_TRAIN_QUANTIZER_H__
#define LM_BUILDER_TRAIN_QUANTIZER_H__

#include "lm/quantize.hh"
#include "util/probing_hash_table.hh"
#include "util/scoped.hh"
#include "util/stream/chain.hh"
#include "util/stream/sort.hh"

#include <boost/noncopyable.hpp>
#include <boost/functional/hash/hash.hpp>

#include <functional>

#include <math.h>
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
class QuantizeCollector : boost::noncopyable {
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
        ++(found->key->count);
      } else {
        current_->count = 1;
        ++current_;
      }
      ++count_;
    }

    void Finish() {
      Flush();
      table_backing_.reset();
      block_.Poison();
    }

    uint64_t Count() const { return count_; }

  private:
    void Flush();

    const std::size_t block_size_;

    util::scoped_malloc table_backing_;
    Table table_;

    FloatCount *current_, *end_;

    util::stream::Link block_;

    uint64_t count_;
};

class QuantizeTrainer : boost::noncopyable {
  public:
    struct Config {
      util::stream::SortConfig sort;
      std::size_t adding_memory; // Amount of memory to use in the Add phase.
      std::size_t block_count;

      Config HalfAdding() const {
        Config ret;
        ret.sort = sort;
        ret.adding_memory = adding_memory / 2;
        ret.block_count = block_count;
        return ret;
      }
    };

    explicit QuantizeTrainer(const Config &config);

    void Add(float value) {
      collector_.Add(value);
    }

    void FinishedAdding() {
      collector_.Finish();
      chain_.Wait(true);
    }

    void Train(float *centers, std::size_t center_count);

  private:
    util::stream::Chain chain_;

    QuantizeCollector collector_;

    util::stream::Sort<CompareFloatCount, CombineFloatCount> sort_;

    std::size_t lazy_memory_;
};

class QuantizeProbBackoff {
  public:
    QuantizeProbBackoff(const QuantizeTrainer::Config &config);

    void Run(const util::stream::ChainPosition &position);

    void Train(ngram::SeparatelyQuantize::Bins *out);

  private:
    QuantizeTrainer prob_, backoff_;
};

class QuantizeProb {
  public:
    QuantizeProb(const QuantizeTrainer::Config &config);

    void Run(const util::stream::ChainPosition &position);

    void Train(ngram::SeparatelyQuantize::Bins &out);

  private:
    QuantizeTrainer prob_;
};

}} // namespaces

#endif // LM_BUILDER_TRAIN_QUANTIZER_H__

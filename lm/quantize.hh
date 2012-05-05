#ifndef LM_QUANTIZE_H__
#define LM_QUANTIZE_H__

#include "lm/blank.hh"
#include "lm/config.hh"
#include "lm/model_type.hh"
#include "util/bit_packing.hh"

#include <algorithm>
#include <vector>

#include <stdint.h>

#include <iostream>

namespace lm {
namespace ngram {

struct Config;

/* Store values directly and don't quantize. */
class DontQuantize {
  public:
    static const ModelType kModelTypeAdd = static_cast<ModelType>(0);
    static void UpdateConfigFromBinary(int, const std::vector<uint64_t> &, Config &) {}
    static std::size_t Size(uint8_t /*order*/, const Config &/*config*/) { return 0; }
    static uint8_t MiddleBits(const Config &/*config*/) { return 63; }
    static uint8_t LongestBits(const Config &/*config*/) { return 31; }

    struct Middle {
      void Write(void *base, uint64_t bit_offset, float prob, float backoff) const {
        util::WriteNonPositiveFloat31(base, bit_offset, prob);
        util::WriteFloat32(base, bit_offset + 31, backoff);
      }
      void Read(const void *base, uint64_t bit_offset, float &prob, float &backoff) const {
        prob = util::ReadNonPositiveFloat31(base, bit_offset);
        backoff = util::ReadFloat32(base, bit_offset + 31);
      }
      void ReadProb(const void *base, uint64_t bit_offset, float &prob) const {
        prob = util::ReadNonPositiveFloat31(base, bit_offset);
      }
      void ReadBackoff(const void *base, uint64_t bit_offset, float &backoff) const {
        backoff = util::ReadFloat32(base, bit_offset + 31);
      }
      uint8_t TotalBits() const { return 63; }
    };

    struct Longest {
      void Write(void *base, uint64_t bit_offset, float prob) const {
        util::WriteNonPositiveFloat31(base, bit_offset, prob);
      }
      void Read(const void *base, uint64_t bit_offset, float &prob) const {
        prob = util::ReadNonPositiveFloat31(base, bit_offset);
      }
      uint8_t TotalBits() const { return 31; }
    };

    DontQuantize() {}

    void SetupMemory(void * /*start*/, const Config & /*config*/) {}

    static const bool kTrain = false;
    // These should never be called because kTrain is false.  
    void Train(uint8_t /*order*/, std::vector<float> &/*prob*/, std::vector<float> &/*backoff*/) {}
    void TrainProb(uint8_t, std::vector<float> &/*prob*/) {}

    void FinishedLoading(const Config &) {}

    Middle Mid(uint8_t /*order*/) const { return Middle(); }
    Longest Long(uint8_t /*order*/) const { return Longest(); }
};

class SeparatelyQuantize {
  private:
    class Bins {
      public:
        // Sigh C++ default constructor
        Bins() {}

        Bins(uint8_t bits, const float *const begin) : begin_(begin), end_(begin_ + (1ULL << bits)), bits_(bits), mask_((1ULL << bits) - 1) {}

        uint64_t EncodeProb(float value) const {
          return Encode(value, 0);
        }

        uint64_t EncodeBackoff(float value) const {
          if (value == 0.0) {
            return HasExtension(value) ? kExtensionQuant : kNoExtensionQuant;
          }
          return Encode(value, 2);
        }

        float Decode(std::size_t off) const { return begin_[off]; }

        uint8_t Bits() const { return bits_; }

        uint64_t Mask() const { return mask_; }

      private:
        uint64_t Encode(float value, size_t reserved) const {
          const float *above = std::lower_bound(begin_ + reserved, end_, value);
          if (above == begin_ + reserved) return reserved;
          if (above == end_) return end_ - begin_ - 1;
          return above - begin_ - (value - *(above - 1) < *above - value);
        }

        const float *begin_;
        const float *end_;
        uint8_t bits_;
        uint64_t mask_;
    };

  public:
    static const ModelType kModelTypeAdd = kQuantAdd;

    static void UpdateConfigFromBinary(int fd, const std::vector<uint64_t> &counts, Config &config);

    static std::size_t Size(uint8_t order, const Config &config) {
      size_t longest_table = (static_cast<size_t>(1) << static_cast<size_t>(config.prob_bits)) * sizeof(float);
      size_t middle_table = (static_cast<size_t>(1) << static_cast<size_t>(config.backoff_bits)) * sizeof(float) + longest_table;
      // unigrams are currently not quantized so no need for a table.  
      return (order - 2) * middle_table + longest_table + /* for the bit counts and alignment padding) */ 8;
    }

    static uint8_t MiddleBits(const Config &config) { return config.prob_bits + config.backoff_bits; }
    static uint8_t LongestBits(const Config &config) { return config.prob_bits; }

    class Middle {
      public:
        Middle(uint8_t prob_bits, const float *prob_begin, uint8_t backoff_bits, const float *backoff_begin) : 
          total_bits_(prob_bits + backoff_bits), total_mask_((1ULL << total_bits_) - 1), prob_(prob_bits, prob_begin), backoff_(backoff_bits, backoff_begin) {}

        void Write(void *base, uint64_t bit_offset, float prob, float backoff) const {
          util::WriteInt57(base, bit_offset, total_bits_, 
              (prob_.EncodeProb(prob) << backoff_.Bits()) | backoff_.EncodeBackoff(backoff));
        }

        void ReadProb(const void *base, uint64_t bit_offset, float &prob) const {
          prob = prob_.Decode(util::ReadInt25(base, bit_offset + backoff_.Bits(), prob_.Bits(), prob_.Mask()));
        }

        void Read(const void *base, uint64_t bit_offset, float &prob, float &backoff) const {
          uint64_t both = util::ReadInt57(base, bit_offset, total_bits_, total_mask_);
          prob = prob_.Decode(both >> backoff_.Bits());
          backoff = backoff_.Decode(both & backoff_.Mask());
        }

        void ReadBackoff(const void *base, uint64_t bit_offset, float &backoff) const {
          backoff = backoff_.Decode(util::ReadInt25(base, bit_offset, backoff_.Bits(), backoff_.Mask()));
        }

        uint8_t TotalBits() const {
          return total_bits_;
        }

      private:
        const uint8_t total_bits_;
        const uint64_t total_mask_;
        const Bins prob_;
        const Bins backoff_;
    };

    class Longest {
      public:
        // Sigh C++ default constructor
        Longest() {}

        Longest(uint8_t prob_bits, const float *prob_begin) : prob_(prob_bits, prob_begin) {}

        void Write(void *base, uint64_t bit_offset, float prob) const {
          util::WriteInt25(base, bit_offset, prob_.Bits(), prob_.EncodeProb(prob));
        }

        void Read(const void *base, uint64_t bit_offset, float &prob) const {
          prob = prob_.Decode(util::ReadInt25(base, bit_offset, prob_.Bits(), prob_.Mask()));
        }

        uint8_t TotalBits() const { return prob_.Bits(); }

      private:
        Bins prob_;
    };

    SeparatelyQuantize() {}

    void SetupMemory(void *start, const Config &config);

    static const bool kTrain = true;
    // Assumes 0.0 is removed from backoff.  
    void Train(uint8_t order, std::vector<float> &prob, std::vector<float> &backoff);
    // Train just probabilities (for longest order).
    void TrainProb(uint8_t order, std::vector<float> &prob);

    void FinishedLoading(const Config &config);

    Middle Mid(uint8_t order) const {
      const float *table = start_ + TableStart(order);
      return Middle(prob_bits_, table, backoff_bits_, table + ProbTableLength());
    }

    Longest Long(uint8_t order) const { return Longest(prob_bits_, start_ + TableStart(order)); }

  private:
    size_t TableStart(uint8_t order) const { return ((1ULL << prob_bits_) + (1ULL << backoff_bits_)) * static_cast<uint64_t>(order - 2); }
    size_t ProbTableLength() const { return (1ULL << prob_bits_); }

    float *start_;
    uint8_t prob_bits_, backoff_bits_;
};

} // namespace ngram
} // namespace lm

#endif // LM_QUANTIZE_H__

#ifndef LM_BUILDER_HEADER_INFO__
#define LM_BUILDER_HEADER_INFO__

#include <string>
#include <stdint.h>

// Some configuration info that is used to add
// comments to the beginning of an ARPA file
struct HeaderInfo {
  const std::string input_file_;
  const uint64_t token_count_;
  const size_t order_;
  const bool interpolate_orders_;

  HeaderInfo(const std::string& input_file, uint64_t token_count, size_t order, bool interpolate_orders)
    : input_file_(input_file), token_count_(token_count), order_(order), interpolate_orders_(interpolate_orders) {}

  // TODO: Add smoothing type
  // TODO: More info if multiple models were interpolated
};

#endif

#ifndef LM_BUILDER_HEADER_INFO__
#define LM_BUILDER_HEADER_INFO__

#include <string>
#include <stdint.h>

// Some configuration info that is used to add
// comments to the beginning of an ARPA file
struct HeaderInfo {
  const std::string input_file;
  const uint64_t token_count;

  HeaderInfo(const std::string& input_file_in, uint64_t token_count_in)
    : input_file(input_file_in), token_count(token_count_in) {}

  // TODO: Add smoothing type
  // TODO: More info if multiple models were interpolated
};

#endif

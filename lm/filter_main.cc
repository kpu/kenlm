#include "lm/filter.hh"
#include "lm/multiple_vocab.hh"

#include <boost/ptr_container/ptr_vector.hpp>

#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>

namespace lm {
namespace {

void DisplayHelp(const char *name) {
  std::cerr
    << "Usage: " << name << " mode input.arpa output.arpa\n\n"
    "copy mode just copies, but makes the format nicer for e.g. irstlm's broken parser.\n\n"
    "single mode computes the vocabulary of stdin and filters to that vocabulary.\n\n"
    "multiple mode computes a separate vocabulary from each line of stdin.  For each line, a separate language is filtered to that line's vocabulary, with the 0-indexed line number appended to the output file name.\n\n"
    "union mode produces one filtered model that is the union of models created by multiple mode.\n\n"
    "context mode is like union mode but replaces the last word with __meta__ if all but the last word match; useful for filtering before backoff weight estimation.\n";
}

} // namespace
} // namespace lm

int main(int argc, char *argv[]) {
  if (argc < 4) {
    lm::DisplayHelp(argv[0]);
    return 1;
  }

  const char *type = argv[1], *in_name = argv[2], *out_name = argv[3];

  if (std::strcmp(type, "copy") && std::strcmp(type, "single") && std::strcmp(type, "multiple") && std::strcmp(type, "union") && std::strcmp(type, "context")) {
    lm::DisplayHelp(argv[0]);
    return 1;
  }

  std::ifstream in_lm(in_name, std::ios::in);
  if (!in_lm) {
    err(2, "Could not open input file %s", in_name);
  }

  if (!std::strcmp(type, "copy")) {
    lm::ARPAOutput out(out_name);
    lm::ReadARPA(in_lm, out);
    return 0;
  }

  if (!std::strcmp(type, "single")) {
    lm::SingleVocabFilter filter(std::cin, out_name);
    lm::ReadARPA(in_lm, filter);
    return 0;
  }

  lm::PrepareMultipleVocab prep;
  lm::ReadMultipleVocab(std::cin, prep);

  if (!std::strcmp(type, "multiple")) {
    lm::MultipleVocabMultipleOutputFilter filter(prep.GetVocabs(), prep.SentenceCount(), out_name);
    lm::ReadARPA(in_lm, filter);
  } else if (!std::strcmp(type, "union")) {
    lm::MultipleVocabSingleOutputFilter filter(prep.GetVocabs(), out_name);
    lm::ReadARPA(in_lm, filter);
  } else if (!std::strcmp(type, "context")) {
    lm::MultipleVocabSingleOutputContextFilter filter(prep.GetVocabs(), out_name);
    lm::ReadARPA(in_lm, filter);
  }
  return 0;
}

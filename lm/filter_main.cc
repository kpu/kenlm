#include "lm/arpa_io.hh"
#include "lm/arpa_filter.hh"
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
    << "Usage: " << name << " mode [context] input.arpa output.arpa\n\n"
    "copy mode just copies, but makes the format nicer for e.g. irstlm's broken\n"
    "    parser.\n"
    "single mode computes the vocabulary of stdin and filters to that vocabulary.\n"
    "multiple mode computes a separate vocabulary from each line of stdin.  For\n"
    "    each line, a separate language is filtered to that line's vocabulary, with\n"
    "    the 0-indexed line number appended to the output file name.\n"
    "union mode produces one filtered model that is the union of models created by\n"
    "    multiple mode.\n\n"
    "If context is specified, only the context (all but last word) has to pass the\n"
    "filter, but the entire n-gram is output.\n";
}

typedef enum { MODE_COPY, MODE_SINGLE, MODE_MULTIPLE, MODE_UNION } FilterMode;

template <class Filter> void RunFilter(std::istream &in_lm, Filter &filter) {
  DispatchARPAInput<Filter> dispatcher(filter);
  ReadARPA(in_lm, dispatcher);
}

template <class Filter> void RunContextFilter(bool context, std::istream &in_lm, Filter &filter) {
  if (context) {
    ContextFilter<Filter> context(filter);
    RunFilter(in_lm, context);
  } else {
    RunFilter(in_lm, filter);
  }
}

void DispatchFilterModes(FilterMode mode, bool context, const char *in_name, const char *out_name) {
  std::ifstream in_lm(in_name, std::ios::in);
  if (!in_lm) {
    err(2, "Could not open input file %s", in_name);
  }

  PrepareMultipleVocab prep;
  if (mode == MODE_MULTIPLE || mode == MODE_UNION) {
    ReadMultipleVocab(std::cin, prep);
  }

  if (mode == MODE_MULTIPLE) {
    MultipleARPAOutput out(out_name, prep.SentenceCount());
    MultipleOutputFilter<MultipleARPAOutput> filter(prep.GetVocabs(), out);
    RunContextFilter(context, in_lm, filter);
    return;
  }

  ARPAOutput out(out_name);

  if (mode == MODE_COPY) {
    ReadARPA(in_lm, out);
    return;
  }

  if (mode == MODE_SINGLE) {
    SingleBinary binary(std::cin);
    SingleOutputFilter<SingleBinary, ARPAOutput> filter(binary, out);
    RunContextFilter(context, in_lm, filter);
    return;
  }

  if (mode == MODE_UNION) {
    UnionBinary binary(prep.GetVocabs());
    SingleOutputFilter<UnionBinary, ARPAOutput> filter(binary, out);
    RunContextFilter(context, in_lm, filter);
    return;
  }
}

} // namespace
} // namespace lm

int main(int argc, char *argv[]) {
  if (argc < 4) {
    lm::DisplayHelp(argv[0]);
    return 1;
  }

  bool context = false;
  boost::optional<lm::FilterMode> mode;
  for (int i = 1; i < argc - 2; ++i) {
    const char *str = argv[i];
    if (!std::strcmp(str, "copy")) {
      mode = lm::MODE_COPY;
    } else if (!std::strcmp(str, "single")) {
      mode = lm::MODE_SINGLE;
    } else if (!std::strcmp(str, "multiple")) {
      mode = lm::MODE_MULTIPLE;
    } else if (!std::strcmp(str, "union")) {
      mode = lm::MODE_UNION;
    } else if (!std::strcmp(str, "context")) {
      context = true;
    } else {
      lm::DisplayHelp(argv[0]);
      return 1;
    }
  }
  
  if (!mode) {
    lm::DisplayHelp(argv[0]);
    return 1;
  }

  lm::DispatchFilterModes(*mode, context, argv[argc - 2], argv[argc - 1]);
  return 0;
}

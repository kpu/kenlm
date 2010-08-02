#include "lm/arpa_io.hh"
#include "lm/filter_format.hh"
#include "lm/filter.hh"
#include "lm/phrase_substrings.hh"
#include "lm/read_vocab.hh"

#include <boost/ptr_container/ptr_vector.hpp>

#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>

namespace lm {
namespace {

void DisplayHelp(const char *name) {
  std::cerr
    << "Usage: " << name << " mode [context] [phrase] [raw|arpa] (vocab|model):input_file output_file\n\n"
    "copy mode just copies, but makes the format nicer for e.g. irstlm's broken\n"
    "    parser.\n"
    "single mode filters to a vocabulary.\n"
    "multiple mode computes a separate vocabulary from each line.  For each line, a\n"
    "    separate language is filtered to that line's vocabulary, with the\n"
    "    0-indexed line number appended to the output file name.\n"
    "union mode produces one filtered model that is the union of models created by\n"
    "    multiple mode.\n\n"
    "context means only the context (all but last word) has to pass the filter, but\n"
    "    the entire n-gram is output.\n\n"
    "phrase means that the vocabulary is actually tab-delimited phrases and that the\n"
    "    phrases can generate the n-gram when assembled in arbitrary order and\n"
    "    clipped.\n\n"
    "The file format is set by [raw|arpa] with default arpa:\n"
    "raw means space-separated tokens, optionally followed by a tab and arbitrary\n"
    "    text.  This is useful for ngram count files.\n"
    "arpa means the ARPA file format for n-gram language models.\n\n"
    "There are two inputs: vocabulary and model.  Either may be given as a file\n"
    "    while the other is on stdin.  Specify the type given as a file using\n"
    "    vocab: or model: before the file name.  \n\n"
    "For ARPA format, the output must be seekable.  For raw format, it can be a\n"
    "    stream i.e. /dev/stdout\n";
}

typedef enum { MODE_COPY, MODE_SINGLE, MODE_MULTIPLE, MODE_UNION } FilterMode;

template <class Format, class Filter> void RunContextFilter(bool context, std::istream &in_lm, Filter filter) {
  if (context) {
    ContextFilter<Filter> context(filter);
    Format::RunFilter(in_lm, context);
  } else {
    Format::RunFilter(in_lm, filter);
  }
}

template <class Format, class Binary> void DispatchBinaryFilter(bool context, std::istream &in_lm, const Binary &binary, typename Format::Output &out) {
  typedef SingleOutputFilter<Binary, typename Format::Output> Filter;
  RunContextFilter<Format, Filter>(context, in_lm, Filter(binary, out));
}

template <class Format> void DispatchFilterModes(FilterMode mode, bool context, bool phrase, std::istream &in_vocab, std::istream &in_lm, const char *out_name) {
  if (mode == MODE_MULTIPLE) {
    typedef MultipleOutputFilter<typename Format::Multiple> Filter;
    boost::unordered_map<std::string, std::vector<unsigned int> > words;
    unsigned int sentence_count = ReadMultipleVocab(in_vocab, words);
    typename Format::Multiple out(out_name, sentence_count);
    RunContextFilter<Format, Filter>(context, in_lm, Filter(words, out));
    return;
  }

  typename Format::Output out(out_name);

  if (mode == MODE_COPY) {
    Format::Copy(in_lm, out);
    return;
  }

  if (mode == MODE_SINGLE) {
    SingleBinary::Words words;
    ReadSingleVocab(in_vocab, words);
    DispatchBinaryFilter<Format, SingleBinary>(context, in_lm, SingleBinary(words), out);
    return;
  }

  if (mode == MODE_UNION) {
    if (phrase) {
      PhraseSubstrings substrings;
      ReadMultiplePhrase(in_vocab, substrings);
      DispatchBinaryFilter<Format, PhraseBinary>(context, in_lm, PhraseBinary(substrings), out);
    } else {
      UnionBinary::Words words;
      ReadMultipleVocab(in_vocab, words);
      DispatchBinaryFilter<Format, UnionBinary>(context, in_lm, UnionBinary(words), out);
    }
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
  bool phrase = false;
  typedef enum {FORMAT_ARPA, FORMAT_COUNT} Format;
  Format format = FORMAT_ARPA;
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
    } else if (!std::strcmp(str, "phrase")) {
      phrase = true;
    } else if (!std::strcmp(str, "context")) {
      context = true;
    } else if (!std::strcmp(str, "arpa")) {
      format = FORMAT_ARPA;
    } else if (!std::strcmp(str, "raw")) {
      format = FORMAT_COUNT;
    } else {
      lm::DisplayHelp(argv[0]);
      return 1;
    }
  }
  
  if (!mode) {
    lm::DisplayHelp(argv[0]);
    return 1;
  }

  if (phrase && mode != lm::MODE_UNION) {
    std::cerr << "Phrase constraint currently only works in union mode." << std::endl;
    return 1;
  }

  bool cmd_is_model = true;
  const char *cmd_input = argv[argc - 2];
  if (!strncmp(cmd_input, "vocab:", 6)) {
    cmd_is_model = false;
    cmd_input += 6;
  } else if (!strncmp(cmd_input, "model:", 6)) {
    cmd_input += 6;
  } else if (strchr(cmd_input, ':')) {
    errx(1, "Specify vocab: or model: before the input file name, not \"%s\"", cmd_input);
  } else {
    std::cerr << "Assuming that " << cmd_input << " is a model file" << std::endl;
  }
  std::ifstream cmd_file(cmd_input, std::ios::in);
  if (!cmd_file) {
    err(2, "Could not open input file %s", cmd_input);
  }

  std::istream *vocab, *model;
  if (cmd_is_model) {
    vocab = &std::cin;
    model = &cmd_file;
  } else {
    vocab = &cmd_file;
    model = &std::cin;
  }

  if (format == FORMAT_ARPA) {
    lm::DispatchFilterModes<lm::ARPAFormat>(*mode, context, phrase, *vocab, *model, argv[argc - 1]);
  } else if (format == FORMAT_COUNT) {
    lm::DispatchFilterModes<lm::CountFormat>(*mode, context, phrase, *vocab, *model, argv[argc - 1]);
  }
  return 0;
}

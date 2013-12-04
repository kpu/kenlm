#include "lm/config.hh"
#include "lm/model.hh"
#include "util/fake_ofstream.hh"
#include "util/file.hh"
#include "util/ersatz_progress.hh"

#include <boost/lexical_cast.hpp>

#include <iostream>
#include <string.h>

namespace lm {

struct NoOpProgress {
  NoOpProgress(uint64_t end) {}
  void operator++() const {}
};

class DumpTrie {
  public:
    DumpTrie() {}

    void Dump(ngram::TrieModel &model, const std::string &base, const std::string &vocab) {
      pieces_.resize(model.GetVocabulary().Bound());
      util::FilePiece f(vocab.c_str());
      try { while (true) {
        StringPiece word = f.ReadDelimited();
        StringPiece &stored = pieces_[model.GetVocabulary().Index(word)];
        if (stored.empty()) {
          stored = StringPiece(static_cast<const char*>(memcpy(pool_.Allocate(word.size()), word.data(), word.size())), word.size());
        }
      } } catch (const util::EndOfFileException &e) {}
      ngram::trie::NodeRange range;
      range.begin = 0;
      range.end = model.GetVocabulary().Bound();
      for (unsigned char i = 0; i < model.Order(); ++i) {
        files_[i].reset(util::CreateOrThrow((base + boost::lexical_cast<std::string>(static_cast<unsigned int>(i) + 1)).c_str()));
        out_[i].SetFD(files_[i].get());
      }
      Dump<util::ErsatzProgress>(model, 1, range);
    }

  private:
    template <class Progress> void Dump(ngram::TrieModel &model, const unsigned char order, const ngram::trie::NodeRange range) {
      ngram::trie::NodeRange pointer;
      ProbBackoff weights;
      WordIndex word;
      Progress progress(range.end);
      for (uint64_t i = range.begin; i < range.end; ++i, ++progress) {
        model.search_.CheckedRead(order, i, word, weights, pointer);
        if (pieces_[word].empty()) continue;
        words_[order - 1] = pieces_[word];
        out_[order - 1] << weights.prob << '\t' << words_[order - 1];
        for (char w = static_cast<char>(order) - 2; w >= 0; --w) {
          out_[static_cast<unsigned char>(order - 1)] << ' ' << words_[static_cast<unsigned char>(w)];
        }
        out_[order - 1] << '\t' << weights.backoff << '\n';
        Dump<NoOpProgress>(model, order + 1, pointer);
      }
    }

    util::Pool pool_;
    std::vector<StringPiece> pieces_;
    util::scoped_fd files_[KENLM_MAX_ORDER];
    util::FakeOFStream out_[KENLM_MAX_ORDER];

    StringPiece words_[KENLM_MAX_ORDER];
};
} // namespace

int main(int argc, char *argv[]) {
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0] << " trie output_base vocab" << std::endl;
    return 1;
  }

  lm::ngram::Config config;
  config.load_method = util::LAZY;
  lm::ngram::TrieModel model(argv[1], config);
  lm::DumpTrie dumper;
  dumper.Dump(model, argv[2], argv[3]);
  return 0;
}

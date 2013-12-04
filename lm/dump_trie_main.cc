#include "lm/config.hh"
#include "lm/enumerate_vocab.hh"
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

class DumpTrie : public EnumerateVocab {
  public:
    DumpTrie() {}

    void Add(WordIndex index, const StringPiece &str) {
      pieces_.push_back(StringPiece((const char*)memcpy(pool_.Allocate(str.size()), str.data(), str.size()), str.size()));
    }

    void Dump(ngram::TrieModel &model, const std::string &base) {
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
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " trie output_base" << std::endl;
    return 1;
  }

  lm::DumpTrie dumper;

  lm::ngram::Config config;
  config.load_method = util::LAZY;
  config.enumerate_vocab = &dumper;
  lm::ngram::TrieModel model(argv[1], config);
  dumper.Dump(model, argv[2]);
  return 0;
}

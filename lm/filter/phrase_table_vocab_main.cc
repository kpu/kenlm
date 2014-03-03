#include "util/fake_ofstream.hh"
#include "util/file_piece.hh"
#include "util/murmur_hash.hh"
#include "util/pool.hh"
#include "util/string_piece.hh"
#include "util/string_piece_hash.hh"
#include "util/tokenize_piece.hh"

#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

#include <cstddef>
#include <vector>

namespace {

struct MutablePiece {
  mutable StringPiece behind;
  bool operator==(const MutablePiece &other) const {
    return behind == other.behind;
  }
};

std::size_t hash_value(const MutablePiece &m) {
  return util::MurmurHashNative(m.behind.data(), m.behind.size());
}

class InternString {
  public:
    const char *Add(StringPiece str) {
      MutablePiece mut;
      mut.behind = str;
      std::pair<boost::unordered_set<MutablePiece>::iterator, bool> res(strs_.insert(mut));
      if (res.second) {
        void *mem = backing_.Allocate(str.size() + 1);
        memcpy(mem, str.data(), str.size());
        static_cast<char*>(mem)[str.size()] = 0;
        res.first->behind = StringPiece(static_cast<char*>(mem), str.size());
      }
      return res.first->behind.data();
    }

  private:
    util::Pool backing_;
    boost::unordered_set<MutablePiece> strs_;
};

struct MurmurChar : public std::unary_function<const char *, std::size_t> {
  std::size_t operator()(const char *value) const {
    return util::MurmurHashNative(&value, sizeof(const char*));
  }
};

class TargetWords {
  private:
    typedef boost::unordered_set<const char *, MurmurChar> Map;
  public:
    void Introduce(StringPiece source) {
      vocab_.resize(vocab_.size() + 1);
      std::vector<unsigned int> temp(1, vocab_.size() - 1);
      Add(temp, source);
    }

    void Add(const std::vector<unsigned int> &sentences, StringPiece target) {
      if (sentences.empty()) return;
      interns_.clear();
      for (util::TokenIter<util::BoolCharacter, true> i(target, util::kSpaces); i; ++i) {
        StringPiece nopipe(i->data(), std::find(i->data(), i->data() + i->size(), '|') - i->data());
        interns_.push_back(intern_.Add(nopipe));
      }
      for (std::vector<unsigned int>::const_iterator i(sentences.begin()); i != sentences.end(); ++i) {
        Map &vocab = vocab_[*i];
        for (std::vector<const char *>::const_iterator j = interns_.begin(); j != interns_.end(); ++j) {
          vocab.insert(*j);
        }
      }
    }

    void Print() const {
      util::FakeOFStream out(1);
      for (std::vector<Map>::const_iterator i = vocab_.begin(); i != vocab_.end(); ++i) {
        for (Map::const_iterator j = i->begin(); j != i->end(); ++j) {
          out << *j << ' ';
        }
        out << '\n';
      }
    }

  private:
    InternString intern_;

    std::vector<Map> vocab_;

    // Temporary in Add.
    std::vector<const char *> interns_;
};

class Input {
  public:
    explicit Input(std::size_t max_length) 
      : max_length_(max_length), sentence_id_(0), empty_() {}

    void AddSentence(StringPiece sentence, TargetWords &targets) {
      targets.Introduce(sentence);

      pieces_.clear();
      for (util::TokenIter<util::BoolCharacter, true> i(sentence, util::kSpaces); i; ++i) {
        StringPiece nopipe(i->data(), std::find(i->data(), i->data() + i->size(), '|') - i->data());
        pieces_.push_back(nopipe);
      }

      for (std::size_t i = 0; i < pieces_.size(); ++i) {
        uint64_t hash = 0;
        for (std::size_t j = i; j < std::min(pieces_.size(), i + max_length_); ++j) {
          hash = util::MurmurHash64A(pieces_[j].data(), pieces_[j].size(), hash);
          map_[hash].push_back(sentence_id_);
        }
      }
      ++sentence_id_;
    }

    const std::vector<unsigned int> &Matches(StringPiece phrase) const {
      uint64_t hash = 0;
      for (util::TokenIter<util::BoolCharacter, true> i(phrase, util::kSpaces); i; ++i) {
        hash = util::MurmurHash64A(i->data(), std::find(i->data(), i->data() + i->size(), '|') - i->data(), hash);
      }
      Map::const_iterator i = map_.find(hash);
      return i == map_.end() ? empty_ : i->second;
    }

  private:
    const std::size_t max_length_;

    // hash of phrase is the key, array of sentences is the value.
    typedef boost::unordered_map<uint64_t, std::vector<unsigned int> > Map;
    Map map_;

    std::size_t sentence_id_;
    
    // Temporary in AddSentence
    std::vector<StringPiece> pieces_;

    const std::vector<unsigned int> empty_;
};

} // namespace

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Expected source text on the command line" << std::endl;
    return 1;
  }
  Input input(7);
  TargetWords targets;
  try {
    util::FilePiece inputs(argv[1], &std::cerr);
    while (true)
      input.AddSentence(inputs.ReadLine(), targets);
  } catch (const util::EndOfFileException &e) {}

  util::FilePiece table(0, NULL, &std::cerr);
  StringPiece line;
  const StringPiece pipes("|||");
  while (true) {
    try {
      line = table.ReadLine();
    } catch (const util::EndOfFileException &e) { break; }
    util::TokenIter<util::MultiCharacter> it(line, pipes);
    StringPiece source(*it);
    if (!source.empty() && source[source.size() - 1] == ' ')
      source.remove_suffix(1);
    targets.Add(input.Matches(source), *++it);
  }
  targets.Print();
}

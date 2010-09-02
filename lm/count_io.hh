#ifndef LM_COUNT_IO_H__
#define LM_COUNT_IO_H__

#include <fstream>
#include <string>

#include <err.h>

namespace lm {

class CountOutput : boost::noncopyable {
  public:
    explicit CountOutput(const char *name) : file_(name, std::ios::out) {}

    void AddNGram(const StringPiece &line) {
      if (!(file_ << line << '\n')) {
        err(3, "Writing counts file failed");
      }
    }

    template <class Iterator> void AddNGram(const Iterator &begin, const Iterator &end, const StringPiece &line) {
      AddNGram(line);
    }

  private:
    std::fstream file_;
};

template <class Output> void ReadCount(std::istream &in_file, Output &out) {
  std::string line;
  while (getline(in_file, line)) {
    util::PieceIterator<'\t'> tabber(line);
    if (!tabber) {
      std::cerr << "Warning: empty n-gram count line being removed\n";
      continue;
    }
    util::PieceIterator<' '> words(*tabber);
    out.AddNGram(words, util::PieceIterator<' '>::end(), line);
  }
  if (!in_file.eof()) {
    err(2, "Reading counts file failed");
  }
}

} // namespace lm

#endif // LM_COUNT_IO_H__

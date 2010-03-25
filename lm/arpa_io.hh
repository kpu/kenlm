#ifndef LM_ARPA_IO_H__
#define LM_ARPA_IO_H__
/* Input and output for ARPA format language model files.
 * TODO: throw exceptions instead of using err.  
 */

#include "util/string_piece.hh"
#include "util/tokenize_piece.hh"

#include <boost/lexical_cast.hpp>
#include <boost/noncopyable.hpp>
#include <boost/progress.hpp>

#include <fstream>
#include <istream>
#include <string>
#include <vector>

#include <err.h>
#include <string.h>

namespace lm {

// Handling for the counts of n-grams at the beginning of ARPA files.
void WriteCounts(std::ostream &out, const std::vector<size_t> &number);
size_t SizeNeededForCounts(const std::vector<size_t> &number);
void ReadCounts(std::istream &in, std::vector<size_t> &number);

// Read and verify the headers like \1-grams: 
void ReadNGramHeader(std::istream &in_lm, unsigned int length);
// Read and verify end marker.  
void ReadEnd(std::istream &in_lm);

/* Writes an ARPA file.  This has to be seekable so the counts can be written
 * at the end.  Hence, I just have it own a std::fstream instead of accepting
 * a separately held std::ostream.  
 */
class ARPAOutput : boost::noncopyable {
  public:
    explicit ARPAOutput(const char *name);

    void ReserveForCounts(std::streampos reserve);

    void BeginLength(unsigned int length);

    inline void AddNGram(const std::string &line) {
      file_ << line << '\n';
      ++fast_counter_;
    }

    template <class Iterator> void AddNGram(unsigned int length, const Iterator &begin, const Iterator &end, const std::string &line) {
      AddNGram(line);
    }

    void EndLength(unsigned int length);

    void Finish();

  private:
    std::fstream file_;
    size_t fast_counter_;
    std::vector<size_t> counts_;
};

template <class Output> void ReadNGrams(std::istream &in, unsigned int length, size_t number, Output &out) {
  std::string line;
  ReadNGramHeader(in, length);
  out.BeginLength(length);
  boost::progress_display display(number, std::cerr, std::string("Length ") + boost::lexical_cast<std::string>(length) + ": " + boost::lexical_cast<std::string>(number) + " total\n");
  for (unsigned int i = 0; i < number;) {
    if (!std::getline(in, line))
      err(2, "Reading ngram failed.  Maybe the counts are wrong?");

    util::PieceIterator<'\t'> tabber(line);
    if (!tabber) {
      std::cerr << "Warning: empty line inside list of " << length << "-grams." << std::endl;
      continue;
    }
    if (!++tabber)
      errx(3, "No tab in line \"%s\"", line.c_str());

    out.AddNGram(length, util::PieceIterator<' '>(*tabber), util::PieceIterator<' '>::end(), line);
    ++i;
    ++display;
  }
  out.EndLength(length);
}

template <class Output> void ReadARPA(std::istream &in_lm, Output &out) {
  std::vector<size_t> number;
  ReadCounts(in_lm, number);
  out.ReserveForCounts(SizeNeededForCounts(number));
  for (unsigned int i = 0; i < number.size(); ++i) {
    ReadNGrams(in_lm, i + 1, number[i], out);
  }
  ReadEnd(in_lm);
  out.Finish();
}

} // namespace lm

#endif // LM_ARPA_IO_H__

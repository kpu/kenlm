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
#include <boost/scoped_array.hpp>

#include <fstream>
#include <istream>
#include <string>
#include <vector>

#include <err.h>
#include <string.h>

namespace util { class FilePiece; }

namespace lm {

class ARPAInputException : public std::exception {
  public:
    explicit ARPAInputException(const StringPiece &message) throw();
    explicit ARPAInputException(const StringPiece &message, const StringPiece &line) throw();
    virtual ~ARPAInputException() throw() {}

    const char *what() const throw() { return what_.c_str(); }

  private:
    std::string what_;
};

class ARPAOutputException : public std::exception {
  public:
    ARPAOutputException(const char *prefix, const std::string &file_name) throw();
    virtual ~ARPAOutputException() throw() {}

    const char *what() const throw() { return what_.c_str(); }

    const std::string &File() const throw() { return file_name_; }

  private:
    std::string what_;
    const std::string file_name_;
};

// Handling for the counts of n-grams at the beginning of ARPA files.
size_t SizeNeededForCounts(const std::vector<size_t> &number);
// TODO: transition to FilePiece.
void ReadCounts(std::istream &in, std::vector<size_t> &number) throw (ARPAInputException);
void ReadCounts(util::FilePiece &in, std::vector<size_t> &number) throw (ARPAInputException);

// Read and verify the headers like \1-grams: 
void ReadNGramHeader(util::FilePiece &in_lm, unsigned int length);
void ReadNGramHeader(std::istream &in_lm, unsigned int length);
// Read and verify end marker.  
void ReadEnd(std::istream &in_lm);

/* Writes an ARPA file.  This has to be seekable so the counts can be written
 * at the end.  Hence, I just have it own a std::fstream instead of accepting
 * a separately held std::ostream.  
 */
class ARPAOutput : boost::noncopyable {
  public:
    explicit ARPAOutput(const char *name, size_t buffer_size = 65536);

    void ReserveForCounts(std::streampos reserve);

    void BeginLength(unsigned int length);

    void AddNGram(const StringPiece &line) {
      try {
        file_ << line << '\n';
      } catch (const std::ios_base::failure &f) {
        throw ARPAOutputException("Writing an n-gram", file_name_);
      }
      ++fast_counter_;
    }

    template <class Iterator> void AddNGram(const Iterator &begin, const Iterator &end, const StringPiece &line) {
      AddNGram(line);
    }

    void EndLength(unsigned int length);

    void Finish();

  private:
    const std::string file_name_;
    boost::scoped_array<char> buffer_;
    std::fstream file_;
    size_t fast_counter_;
    std::vector<size_t> counts_;
};


template <class Output> void ReadNGrams(std::istream &in, unsigned int length, size_t max_length, size_t number, Output &out) {
  std::string line;
  ReadNGramHeader(in, length);
  out.BeginLength(length);
  boost::progress_display display(number, std::cerr, std::string("Length ") + boost::lexical_cast<std::string>(length) + "/" + boost::lexical_cast<std::string>(max_length) + ": " + boost::lexical_cast<std::string>(number) + " total\n");
  for (unsigned int i = 0; i < number;) {
    if (!std::getline(in, line)) throw ARPAInputException("Reading ngram failed.  Maybe the counts are wrong?");

    util::PieceIterator<'\t'> tabber(line);
    if (!tabber) {
      std::cerr << "Warning: empty line inside list of " << length << "-grams." << std::endl;
      continue;
    }
    if (!++tabber) throw ARPAInputException("no tab", line);

    out.AddNGram(util::PieceIterator<' '>(*tabber), util::PieceIterator<' '>::end(), line);
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
    ReadNGrams(in_lm, i + 1, number.size(), number[i], out);
  }
  ReadEnd(in_lm);
  out.Finish();
}

} // namespace lm

#endif // LM_ARPA_IO_H__

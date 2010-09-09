#ifndef LM_EXCEPTION_HH__
#define LM_EXCEPTION_HH__

#include "util/errno_exception.hh"
#include "util/string_piece.hh"

#include <exception>
#include <string>

namespace lm {

class NotFoundInVocabException : public std::exception {
  public:
    explicit NotFoundInVocabException(const StringPiece &word) throw();

    ~NotFoundInVocabException() throw() {}

    const std::string &Word() const throw() { return word_; }

    virtual const char *what() const throw() { return what_.c_str(); }

  private:
    std::string word_;
    std::string what_;
};

class LoadException : public std::exception {
   public:
      virtual ~LoadException() throw() {}

   protected:
      LoadException() throw() {}
};

class VocabLoadException : public LoadException {
  public:
    virtual ~VocabLoadException() throw() {}

  protected:
    VocabLoadException() throw() {}
};

// Different words, same ids
class IDDuplicateVocabLoadException : public VocabLoadException {
   public:
      IDDuplicateVocabLoadException(unsigned int id, const StringPiece &existing, const StringPiece &replacement) throw();

      ~IDDuplicateVocabLoadException() throw() {}

      const char *what() const throw() { return what_.c_str(); }

   private:
      std::string what_;
};

// One word, two ids.
class WordDuplicateVocabLoadException : public VocabLoadException {
   public:
      WordDuplicateVocabLoadException(const StringPiece &word, unsigned int first, unsigned int second) throw();

      ~WordDuplicateVocabLoadException() throw() {}

      const char *what() const throw() { return what_.c_str(); }

   private:
      std::string what_;
};

class AllocateMemoryLoadException : public util::ErrnoException {
  public:
    explicit AllocateMemoryLoadException(size_t requested) throw();

    ~AllocateMemoryLoadException() throw() {}
};

class OpenFileLoadException : public LoadException {
  public:
    explicit OpenFileLoadException(const char *name) throw() : name_(name) {
      what_ = "Error opening file ";
      what_ += name;
    }

    ~OpenFileLoadException() throw() {}

    const char *what() const throw() { return what_.c_str(); }

  private:
    std::string name_;
    std::string what_;
};

class ReadFileLoadException : public LoadException {
  public:
    explicit ReadFileLoadException(const char *name) throw() : name_(name) {
      what_ = "Error reading file ";
      what_ += name;
    }

    ~ReadFileLoadException() throw() {}

    const char *what() const throw() { return what_.c_str(); }

  private:
    std::string name_;
    std::string what_;
};

class FormatLoadException : public LoadException {
  public:
    explicit FormatLoadException(const StringPiece &complaint, const StringPiece &context = StringPiece()) throw();

    ~FormatLoadException() throw() {}

    const char *what() const throw() { return what_.c_str(); }

  private:
    std::string what_;
};

class SpecialWordMissingException : public LoadException {
  public:
    virtual ~SpecialWordMissingException() throw() {}

  protected:
    SpecialWordMissingException() throw() {}
};

class BeginSentenceMissingException : public SpecialWordMissingException {
  public:
    BeginSentenceMissingException() throw() {}

    ~BeginSentenceMissingException() throw() {}

    const char *what() const throw() { return "Begin of sentence marker missing from vocabulary"; }
};

class EndSentenceMissingException : public SpecialWordMissingException {
  public:
    EndSentenceMissingException() throw() {}

    ~EndSentenceMissingException() throw() {}

    const char *what() const throw() { return "End of sentence marker missing from vocabulary"; }
};

class UnknownMissingException : public SpecialWordMissingException {
  public:
    UnknownMissingException() throw() {}

    ~UnknownMissingException() throw() {}

    const char *what() const throw() { return "Unknown word missing from vocabulary"; }
};


} // namespace lm

#endif

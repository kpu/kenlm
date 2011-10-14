#ifndef UTIL_TOKENIZE_PIECE__
#define UTIL_TOKENIZE_PIECE__

#include "util/string_piece.hh"

#include <boost/iterator/iterator_facade.hpp>

#include <algorithm>
#include <iostream>

/* Usage:
 *
 * for (PieceIterator<' '> i(" foo \r\n bar "); i; ++i) {
 *   std::cout << *i << "\n";
 * }
 *
 */

namespace util {

// Tokenize a StringPiece using an iterator interface.  boost::tokenizer doesn't work with StringPiece.
template <char d> class PieceIterator : public boost::iterator_facade<PieceIterator<d>, const StringPiece, boost::forward_traversal_tag> {
  public:
    // Default construct is end, which is also accessed by kEndPieceIterator;
    PieceIterator() {}

    explicit PieceIterator(const StringPiece &str)
      : after_(str) {
        increment();
      }

    bool operator!() const {
      return after_.data() == 0;
    }
    operator bool() const {
      return after_.data() != 0;
    }

    static PieceIterator<d> end() {
      return PieceIterator<d>();
    }

  private:
    friend class boost::iterator_core_access;

    void increment() {
      const char *start = after_.data();
      for (; (start != after_.data() + after_.size()) && (d == *start); ++start) {}
      if (start == after_.data() + after_.size()) {
        // End condition.
        after_.clear();
        return;
      }
      const char *finish = start;
      for (; (finish != after_.data() + after_.size()) && (d != *finish); ++finish) {}
      current_ = StringPiece(start, finish - start);
      after_ = StringPiece(finish, after_.data() + after_.size() - finish);
    }

    bool equal(const PieceIterator &other) const {
      return after_.data() == other.after_.data();
    }

    const StringPiece &dereference() const { return current_; }

    StringPiece current_;
    StringPiece after_;
};

class MultiCharacter {
  public:
    explicit MultiCharacter(const StringPiece &delimiter) : delimiter_(delimiter) {}

    StringPiece Find(const StringPiece &in) const {
      return StringPiece(std::search(in.data(), in.data() + in.size(), delimiter_.data(), delimiter_.data() + delimiter_.size()), delimiter_.size());
    }

  private:
    StringPiece delimiter_;
};

class AnyCharacter {
  public:
    explicit AnyCharacter(const StringPiece &chars) : chars_(chars) {}

    StringPiece Find(const StringPiece &in) const {
      return StringPiece(std::find_first_of(in.data(), in.data() + in.size(), chars_.data(), chars_.data() + chars_.size()), 1);
    }

  private:
    StringPiece chars_;
};

template <class Find, bool SkipEmpty = false> class TokenIter : public boost::iterator_facade<TokenIter<Find, SkipEmpty>, const StringPiece, boost::forward_traversal_tag> {
  public:
    TokenIter() {}

    TokenIter(const StringPiece &str, const Find &finder) : after_(str), finder_(finder) {
      increment();
    }

    bool operator!() const {
      return current_.data() == 0;
    }
    operator bool() const {
      return current_.data() != 0;
    }

    static TokenIter<Find> end() {
      return TokenIter<Find>();
    }

  private:
    friend class boost::iterator_core_access;

    void increment() {
      do {
        StringPiece found(finder_.Find(after_));
        current_ = StringPiece(after_.data(), found.data() - after_.data());
        if (found.data() == after_.data() + after_.size()) {
          after_ = StringPiece(NULL, 0);
        } else {
          after_ = StringPiece(found.data() + found.size(), after_.data() - found.data() + after_.size() - found.size());
        }
      } while (SkipEmpty && current_.data() && current_.empty()); // Compiler should optimize this away if SkipEmpty is false.  
    }

    bool equal(const TokenIter<Find> &other) const {
      return after_.data() == other.after_.data();
    }

    const StringPiece &dereference() const {
      return current_;
    }

    StringPiece current_;
    StringPiece after_;

    Find finder_;
};

} // namespace util

#endif // UTIL_TOKENIZE_PIECE__

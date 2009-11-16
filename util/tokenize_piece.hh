#ifndef UTIL_TOKENIZE_PIECE__
#define UTIL_TOKENIZE_PIECE__

#include "util/string_piece.hh"

#include <boost/iterator/iterator_facade.hpp>

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

} // namespace util

#endif // UTIL_TOKENIZE_PIECE__

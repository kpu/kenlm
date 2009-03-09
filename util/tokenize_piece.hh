#ifndef UTIL_TOKENIZE_PIECE__
#define UTIL_TOKENIZE_PIECE__

#include "util/string_piece.hh"

#include <boost/iterator/iterator_facade.hpp>

#include <bitset>

/* Usage:
 *
 * AnyCharacterDelimiter delimit(" \r\n\t");
 * for (PieceIterator i(" foo \r\n bar ", delimit); i; ++i) {
 *   std::cout << *i << "\n";
 * }
 *
 */

namespace util {

class AnyCharacterDelimiter {
	public:
		explicit AnyCharacterDelimiter(const StringPiece &delimit) {
			for (StringPiece::iterator i = delimit.begin(); i != delimit.end(); ++i) {
				delimit_[*i] = 1;
			}
		}

		bool operator()(const unsigned char val) const {
			return delimit_[val];
		}

	private:
		std::bitset<256> delimit_;
};

// Tokenize a StringPiece using an iterator interface.  boost::tokenizer doesn't work with StringPiece.
class PieceIterator : public boost::iterator_facade<PieceIterator, const StringPiece, boost::forward_traversal_tag> {
	public:
		// Default construct is end, which is also accessed by kEndPieceIterator;
		PieceIterator() {}

		// delimiter must exist for life of iterator, sadly.
		PieceIterator(const StringPiece &str, const AnyCharacterDelimiter &delimit)
			  : after_(str), delimit_(&delimit) {
			increment();
		}

		bool operator!() const {
			return after_.data() == 0;
		}
		operator bool() const {
			return after_.data() != 0;
		}

	private:
		friend class boost::iterator_core_access;

		void increment() {
			StringPiece::iterator start(after_.begin());
			for (; start != after_.end() && (*delimit_)(*start); ++start) {}
			if (start == after_.end()) {
				// End condition.
				after_.clear();
				return;
			}
			StringPiece::iterator finish(start);
			for (; finish != after_.end() && !(*delimit_)(*finish); ++finish) {}
			current_.set(start, finish - start);
			after_.set(finish, after_.end() - finish);
		}

		bool equal(const PieceIterator &other) const {
			return after_.data() == other.after_.data();
		}

		const StringPiece &dereference() const { return current_; }

		StringPiece current_;
		StringPiece after_;
		const AnyCharacterDelimiter *delimit_;
};

// StringPiece is simple enough that this can be static.
extern const PieceIterator kEndPieceIterator;

} // namespace util

#endif // UTIL_TOKENIZE_PIECE__

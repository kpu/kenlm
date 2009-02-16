#include "util/utf8.hh"

#include <unicode/uchar.h>
#include <unicode/utf8.h>

namespace utf8 {

// Could be more efficient, but I'm not terribly worried about that.
NotUTF8Exception::NotUTF8Exception(const StringPiece &original) throw()
	: original_(original.data(), original.length()),
	  what_(std::string("Bad UTF-8: ") + original_) {}

bool IsPunctuation(const StringPiece &str) throw(NotUTF8Exception) {
	// TODO: is this MEMT-hack desirable?
	if (str[0] == '\'' || str == "n't") return true;
	int32_t offset = 0;
	int32_t length = static_cast<uint32_t>(str.size());
	while (offset < length) {
		UChar32 character;
		U8_NEXT(str.data(), offset, (int32_t)str.size(), character);
		if (character < 0) {
			throw NotUTF8Exception(str);
		}
		if (!u_ispunct(character)) return false;
	}
	return true;
}

} // namespace utf8

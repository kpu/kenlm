#include "lm/ngram.hh"
#include "util/tokenize_piece.hh"

#include <boost/functional/hash/hash.hpp>
#include <boost/lexical_cast.hpp>

#include <algorithm>
#include <fstream>
#include <string>

#include <stdlib.h>

namespace lm {
namespace ngram {
namespace detail {

// All of the entropy is in low order bits and boost::hash does poorly with these.
// These numbers came from mashing on the keyboard and were chosen to be near 2^64.
inline uint64_t CombineWordHash(uint64_t current, const uint32_t next) {
	return (current * 8978948897894561157ULL) ^ (static_cast<uint64_t>(next) * 17894857484156487943ULL);
}

void ChainedWordHash(const uint32_t *word, const uint32_t *word_end, uint64_t *out) {
	if (word == word_end) return;
	uint64_t current = static_cast<uint64_t>(*word);
	for (++word; word != word_end; ++word, ++out) {
		current = CombineWordHash(current, *word);
		*out = current;
	}
}

uint64_t ChainedWordHash(const uint32_t *word, const uint32_t *word_end) {
	if (word == word_end) return 0;
	uint64_t current = static_cast<uint64_t>(*word);
	for (++word; word != word_end; ++word) {
		current = CombineWordHash(current, *word);
	}
	return current;
}

inline float FindBackoff(const boost::unordered_map<uint64_t, ProbBackoff, IdentityHash> &in, uint64_t key) {
	boost::unordered_map<uint64_t, ProbBackoff, IdentityHash>::const_iterator i = in.find(key);
	if (i == in.end()) {
		return 0.0;
	} else {
		return i->second.backoff;
	}
}

void ParseDataCounts(std::istream &f, std::vector<size_t> &counts) {
	util::AnyCharacterDelimiter equals_delim(" =");
	std::string line;
	for (unsigned int order = 1; getline(f, line) && !line.empty(); ++order) {
		util::PieceIterator it(line, equals_delim);
		if (!it || (*it != "ngram") || !(++it))
			throw FormatLoadException("expected ngram length line", line);
		if (boost::lexical_cast<unsigned int>(*it) != order) {
			std::string message("expected order ");
			message += boost::lexical_cast<std::string>(order);
			message += " but received ";
			it->AppendToString(&message);
			throw FormatLoadException(message, line);
		}
		if (!(++it))
			throw FormatLoadException("expected ngram count", line);
		counts.push_back(boost::lexical_cast<size_t>(*it));
		if (++it)
			throw FormatLoadException("too many tokens in ngram count line", line);
	}
}

struct VocabularyFriend {
	static WordIndex InsertUnique(Vocabulary &vocab, std::auto_ptr<std::string> &word) {
		return vocab.InsertUnique(word);
	}
	static void FinishedLoading(Vocabulary &vocab) {
		vocab.FinishedLoading();
	}
};

void Read1Grams(std::fstream &f, const size_t count, Vocabulary &vocab, std::vector<ProbBackoff> &unigrams) {
	std::string line;
	while (getline(f, line) && line != "\\1-grams:") {}
	if (!f) throw FormatLoadException("Did not get \\1-grams: line");
	// Special unigram reader because unigram's data structure is different and because we're inserting vocab words.
	std::auto_ptr<std::string> unigram(new std::string);
	for (size_t i = 0; i < count; ++i) {
		float prob;
		f >> prob;
		if (f.get() != '\t')
			throw FormatLoadException("Expected tab after probability");
		f >> *unigram;
		if (!f) throw FormatLoadException("Actual unigram count less than reported");
		ProbBackoff &ent = unigrams[VocabularyFriend::InsertUnique(vocab, unigram)];
		ent.prob = prob;
		int delim = f.get();
		if (!f) throw FormatLoadException("Missing line termination while reading unigrams");
		if (delim == '\t') {
			if (!(f >> ent.backoff)) throw FormatLoadException("Failed to read backoff");
			if ((f.get() != '\n') || !f) throw FormatLoadException("Expected newline after backoff");
		} else if (delim != '\n') {
			ent.backoff = 0.0;
			throw FormatLoadException("Expected tab or newline after unigram");
		}
		unigram.reset(new std::string);
	}
	if (getline(f, line)) FormatLoadException("Blank line after ngrams missing");
	if (!line.empty()) throw FormatLoadException("Blank line after ngrams not blank", line);
	VocabularyFriend::FinishedLoading(vocab);
}

void SetNGramEntry(boost::unordered_map<uint64_t, detail::ProbBackoff, detail::IdentityHash> &place, uint64_t key, float prob, float backoff) {
	if (__builtin_expect(!place.insert(std::make_pair(key, ProbBackoff(prob, backoff))).second, 0))
		std::cerr << "Warning: hash collision with " << key <<  std::endl;
}

void SetNGramEntry(boost::unordered_map<uint64_t, detail::ProbBackoff, detail::IdentityHash> &place, uint64_t key, float prob) {
	if (__builtin_expect(!place.insert(std::make_pair(key, ProbBackoff(prob, 0.0))).second, 0))
		std::cerr << "Warning: hash collision with " << key << std::endl;
}
void SetNGramEntry(boost::unordered_map<uint64_t, detail::Prob, detail::IdentityHash> &place, uint64_t key, float prob, float backoff) {
	throw FormatLoadException("highest order n-gram has a backoff listed");
}
void SetNGramEntry(boost::unordered_map<uint64_t, detail::Prob, detail::IdentityHash> &place, uint64_t key, float prob) {
	if (__builtin_expect(!place.insert(std::make_pair(key, Prob(prob))).second, 0))
		std::cerr << "Warning: hash collision with " << key << std::endl;
}

template <class Place> void ReadNGrams(std::fstream &f, const unsigned int n, const size_t count, const Vocabulary &vocab, Place &place) {
	std::string line;
	if (!getline(f, line)) throw FormatLoadException("Error reading \\ngram header");
	std::string expected("\\");
	expected += boost::lexical_cast<std::string>(n) += "-grams:";
	if (line != expected) throw FormatLoadException(std::string("Expected header \"") + expected + "\"", line);

	util::AnyCharacterDelimiter tab_delim("\t");
	util::AnyCharacterDelimiter space_delim(" ");

	std::cerr << "Fail?" << std::endl;

	// vocab ids of words in reverse order
	uint32_t vocab_ids[n];
	for (size_t i = 0; i < count; ++i) {
		std::cerr << "Reading " << i << " in " << n << std::endl;
		if (!getline(f, line)) throw FormatLoadException("Reading ngram line");
		std::cerr << line << std::endl;
		util::PieceIterator tab_it(line, tab_delim);
		if (!tab_it) throw FormatLoadException("Blank n-gram line", line);
		float prob = boost::lexical_cast<float>(*tab_it);
		
		if (!(++tab_it)) throw FormatLoadException("Missing words", line);
		uint32_t *vocab_out = &vocab_ids[n-1];
		for (util::PieceIterator space_it(*tab_it, space_delim); space_it; ++space_it, --vocab_out) {
			if (vocab_out < vocab_ids) throw FormatLoadException("Too many words", line);
			*vocab_out = vocab.Index(*space_it);
		}
		std::cerr << "Made it through word loop." << std::endl;
		if (vocab_out + 1 != vocab_ids) throw FormatLoadException("Too few words", line);
		uint64_t key = ChainedWordHash(vocab_ids, vocab_ids + n);

		std::cerr << "Hashed." << std::endl;

		if (++tab_it) {
			std::cerr << "Reading backoff " << std::endl;
			float backoff = boost::lexical_cast<float>(*tab_it);
			std::cerr << *tab_it << " maps to " << backoff << std::endl;
		        if (++tab_it) throw FormatLoadException("Too many columns", line);
			std::cerr << "Post second increment" << std::endl;
			SetNGramEntry(place, key, prob, backoff);
		} else {
			std::cerr << "Setting without backoff " << std::endl;
			SetNGramEntry(place, key, prob);
		}
		std::cerr << "Back from SetNGramEntry" << std::endl;
	}
	if (getline(f, line)) FormatLoadException("Blank line after ngrams missing");
	if (!line.empty()) throw FormatLoadException("Blank line after ngrams not blank", line);
}

} // namespace detail

Model::Model(const char *arpa) {
	std::fstream f(arpa, std::ios::in);
	if (!f) throw OpenFileLoadException(arpa);
	f.exceptions(std::fstream::failbit | std::fstream::badbit);

	std::string line;
	while (getline(f, line) && line != "\\data\\") {}
	if (!f) throw FormatLoadException("Did not get data line");
	// counts[n-1] is number of n-grams.
	std::vector<size_t> counts;
	detail::ParseDataCounts(f, counts);

	if (counts.size() < 2)
		throw FormatLoadException("This ngram implementation assumes at least a bigram model.");
	order_ = counts.size();

	std::cerr << "Read counts. " << std::endl;

	const float kLoadFactor = 1.0;
	unigram_.resize(counts[0]);
	middle_vec_.resize(counts.size() - 2);
	for (unsigned int n = 2; n < counts.size(); ++n) {
		middle_vec_[n-2].rehash(1 + static_cast<size_t>(static_cast<float>(counts[n-1]) / kLoadFactor));
	}
	longest_.rehash(1 + static_cast<size_t>(static_cast<float>(counts[counts.size() - 1]) / kLoadFactor));
	std::cerr << "Rehashed. " << std::endl;

	Read1Grams(f, counts[0], vocab_, unigram_);
	std::cerr << "Read unigrams. " << std::endl;
	for (unsigned int n = 2; n < counts.size(); ++n) {
		std::cerr << "Reading " << n << std::endl;
		ReadNGrams(f, n, counts[n-1], vocab_, middle_vec_[n-2]);
	}
	std::cerr << "Read middle" << std::endl;
	ReadNGrams(f, counts.size(), counts[counts.size() - 1], vocab_, longest_);
	std::cerr << "Ready" << std::endl;
}

// Assumes order at least 2.
LogDouble Model::InternalIncrementalScore(const uint32_t *words, unsigned int &ngram_length) const {
	// loookup_hashes[n-2] is the hash for n-gram, appropriate for lookup in
	// middle_vec_[n-2] if 2 <= n < order_
	// longest_ if n == order_
	uint64_t lookup_hashes[order_ - 1];
	detail::ChainedWordHash(words, words + order_, lookup_hashes);

	const uint64_t *hash_ent = &lookup_hashes[order_ - 2];
	{
		Longest::const_iterator found(longest_.find(*hash_ent));
		if (found != longest_.end()) {
			ngram_length = order_;
			return LogDouble(AlreadyLogTag(), found->second.prob);
		}
	}
	--hash_ent;

	// backoff_hashes[n-2] is the hash of the n-gram at the end of history.  Use in
	// middle_vec_[n-2] if 2 <= n < order_.
	uint64_t backoff_hashes[order_ - 2];
	detail::ChainedWordHash(words + 1, words + order_, backoff_hashes);

	// Start at the end of backoff_hashes.  This should be safe since it's not derefenced until inside the loop.
	const uint64_t *backoff_ent = backoff_hashes + order_ - 3;
	float backoff = 0.0;
	for (std::vector<Middle>::const_reverse_iterator mid(middle_vec_.rbegin()); mid != middle_vec_.rend(); ++mid, --hash_ent, --backoff_ent) {
		backoff += FindBackoff(*mid, *backoff_ent);
		Middle::const_iterator found(mid->find(*hash_ent));
		if (found != mid->end()) {
			ngram_length = hash_ent - lookup_hashes + 2;
			return LogDouble(AlreadyLogTag(), backoff + found->second.prob);
		}
	}

	backoff += unigram_[words[1]].backoff;

	if (words[0] == vocab_.NotFound()) {
		ngram_length = 0;
	} else {
		ngram_length = 1;
	}
	// Unigram.
	return LogDouble(AlreadyLogTag(), backoff + unigram_[words[0]].prob);
}

} // namespace ngram
} // namespace lm

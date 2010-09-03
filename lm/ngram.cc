#include "lm/arpa_io.hh"
#include "lm/ngram.hh"
#include "util/tokenize_piece.hh"

#include <boost/functional/hash/hash.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/progress.hpp>

#include <algorithm>
#include <fstream>
#include <string>

#include <cmath>

#include <stdlib.h>

namespace lm {
namespace ngram {

WordIndex Vocabulary::InsertUnique(std::string *word) {
  std::pair<boost::unordered_map<StringPiece, WordIndex>::const_iterator, bool> res(ids_.insert(std::make_pair(StringPiece(*word), available_)));
  if (__builtin_expect(!res.second, 0)) {
    delete word;
    throw WordDuplicateVocabLoadException(*word, res.first->second, available_);
  }
  strings_.push_back(word);
  return available_++;
}

void Vocabulary::FinishedLoading() {
  if (ids_.find(StringPiece("<s>")) == ids_.end()) throw BeginSentenceMissingException();
  if (ids_.find(StringPiece("</s>")) == ids_.end()) throw EndSentenceMissingException();
  // Allow lowercase form of unknown if found, otherwise complain.  It's better to not tolerate an   LM without OOV.   
  if (ids_.find(StringPiece("<unk>")) == ids_.end()) {
    if (ids_.find(StringPiece("<UNK>")) == ids_.end()) {
      // TODO: throw up unless there's a command line option saying not to.
      //throw UnknownMissingException();
      InsertUnique(new std::string("<unk>"));
    } else {
      ids_["<unk>"] = Index(StringPiece("<UNK>"));
    }
  }
  SetSpecial(Index(StringPiece("<s>")), Index(StringPiece("</s>")), Index(StringPiece("<unk>")),     available_);
}

namespace detail {

// All of the entropy is in low order bits and boost::hash does poorly with these.
// Odd numbers near 2^64 chosen by mashing on the keyboard.  
inline uint64_t CombineWordHash(uint64_t current, const uint32_t next) {
	uint64_t ret = (current * 8978948897894561157ULL) ^ (static_cast<uint64_t>(next) * 17894857484156487943ULL);
	return ret;
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

struct VocabularyFriend {
	static void Reserve(Vocabulary &vocab, size_t to) {
		vocab.Reserve(to);
	}
	static WordIndex InsertUnique(Vocabulary &vocab, std::string *word) {
		return vocab.InsertUnique(word);
	}
	static void FinishedLoading(Vocabulary &vocab) {
		vocab.FinishedLoading();
	}
};

void Read1Grams(std::fstream &f, const size_t count, Vocabulary &vocab, std::vector<ProbBackoff> &unigrams) {
  ReadNGramHeader(f, 1);
	boost::progress_display progress(count, std::cerr, "Loading 1-grams\n");
  // +1 in case OOV is not found.
	VocabularyFriend::Reserve(vocab, count + 1);
	std::string line;
	// Special unigram reader because unigram's data structure is different and because we're inserting vocab words.
	std::auto_ptr<std::string> unigram(new std::string);
	for (size_t i = 0; i < count; ++i) {
		float prob;
		f >> prob;
		if (f.get() != '\t')
			throw FormatLoadException("Expected tab after probability");
		f >> *unigram;
		if (!f) throw FormatLoadException("Actual unigram count less than reported");
		ProbBackoff &ent = unigrams[VocabularyFriend::InsertUnique(vocab, unigram.release())];
		unigram.reset(new std::string);
		ent.prob = prob * M_LN10;
		int delim = f.get();
		if (!f) throw FormatLoadException("Missing line termination while reading unigrams");
		if (delim == '\t') {
			if (!(f >> ent.backoff)) throw FormatLoadException("Failed to read backoff");
			ent.backoff *= M_LN10;
			if ((f.get() != '\n') || !f) throw FormatLoadException("Expected newline after backoff");
		} else if (delim == '\n') {
			ent.backoff = 0.0;
		} else {
			ent.backoff = 0.0;
			throw FormatLoadException("Expected tab or newline after unigram");
		}
		++progress;
	}
	if (getline(f, line)) FormatLoadException("Blank line after ngrams missing");
	if (!line.empty()) throw FormatLoadException("Blank line after ngrams not blank", line);
	VocabularyFriend::FinishedLoading(vocab);
}

void SetNGramEntry(boost::unordered_map<uint64_t, ProbBackoff, IdentityHash> &place, uint64_t key, float prob, float backoff) {
	if (__builtin_expect(!place.insert(std::make_pair(key, ProbBackoff(prob, backoff))).second, 0))
		std::cerr << "Warning: hash collision with " << key <<  std::endl;
}

void SetNGramEntry(boost::unordered_map<uint64_t, ProbBackoff, IdentityHash> &place, uint64_t key, float prob) {
	if (__builtin_expect(!place.insert(std::make_pair(key, ProbBackoff(prob, 0.0))).second, 0))
		std::cerr << "Warning: hash collision with " << key << std::endl;
}
void SetNGramEntry(boost::unordered_map<uint64_t, Prob, IdentityHash> &place, uint64_t key, float prob, float backoff) {
	throw FormatLoadException("highest order n-gram has a backoff listed");
}
void SetNGramEntry(boost::unordered_map<uint64_t, Prob, IdentityHash> &place, uint64_t key, float prob) {
	if (__builtin_expect(!place.insert(std::make_pair(key, Prob(prob))).second, 0))
		std::cerr << "Warning: hash collision with " << key << std::endl;
}

template <class Place> void ReadNGrams(std::fstream &f, const unsigned int n, const size_t count, const Vocabulary &vocab, Place &place) {
	boost::progress_display progress(count, std::cerr, std::string("Loading ") + boost::lexical_cast<std::string>(n) + "-grams\n");

  ReadNGramHeader(f, n);

	// vocab ids of words in reverse order
	uint32_t vocab_ids[n];
  std::string word;
	for (size_t i = 0; i < count; ++i) {
    try {
      float prob, backoff;
      f >> prob;
      for (uint32_t *vocab_out = &vocab_ids[n-1]; vocab_out >= vocab_ids; --vocab_out) {
        f >> word;
        *vocab_out = vocab.Index(word);
      }
      uint64_t key = ChainedWordHash(vocab_ids, vocab_ids + n);

      switch (f.get()) {
        case '\t':
          f >> backoff;
          SetNGramEntry(place, key, prob * M_LN10, backoff * M_LN10);
          break;
        case '\n':
          SetNGramEntry(place, key, prob * M_LN10);
          break;
        default:
          throw FormatLoadException("Got unexpected delimiter before backoff weight");
      }
      ++progress;
    } catch (const std::ios_base::failure &f) {
      throw FormatLoadException("Error reading the " + boost::lexical_cast<std::string>(i) + "th " + boost::lexical_cast<std::string>(n) + "-gram.");
    }
	}

  std::string line;
	if (!getline(f, line)) FormatLoadException("Blank line after ngrams missing");
	if (!line.empty()) throw FormatLoadException("Blank line after ngrams not blank", line);
}

} // namespace detail

Model::Model(const char *arpa, bool print_status) {
	std::fstream f(arpa, std::ios::in);
	if (!f) throw OpenFileLoadException(arpa);
	f.exceptions(std::fstream::failbit | std::fstream::badbit);

	std::vector<size_t> counts;
	ReadCounts(f, counts);

	if (counts.size() < 2)
		throw FormatLoadException("This ngram implementation assumes at least a bigram model.");
	if (counts.size() > kMaxOrder)
		throw FormatLoadException(std::string("Edit ngram.hh and change kMaxOrder to at least ") + boost::lexical_cast<std::string>(counts.size()));
	order_ = counts.size();

	// Make space in the data structures
	const float kLoadFactor = 1.0;
  // in case OOV needs to be added.
  unigram_.reserve(counts[0] + 1);
	unigram_.resize(counts[0]);
	middle_vec_.resize(counts.size() - 2);
	for (unsigned int n = 2; n < counts.size(); ++n) {
		middle_vec_[n-2].rehash(1 + static_cast<size_t>(static_cast<float>(counts[n-1]) / kLoadFactor));
	}
	longest_.rehash(1 + static_cast<size_t>(static_cast<float>(counts[counts.size() - 1]) / kLoadFactor));

	// Read the unigrams.
	Read1Grams(f, counts[0], vocab_, unigram_);
	if (std::fabs(unigram_[vocab_.NotFound()].backoff) > 0.0000001) {
		throw FormatLoadException(std::string("Backoff for unknown word with index ") + boost::lexical_cast<std::string>(vocab_.NotFound()) + " is " + boost::lexical_cast<std::string>(unigram_[vocab_.NotFound()].backoff) + std::string(" not zero"));
  }
	begin_sentence_backoff_ = unigram_[vocab_.BeginSentence()].backoff;
	if (print_status) std::cerr << "Loaded unigrams" << std::endl;
	
	// Read the n-grams.
	for (unsigned int n = 2; n < counts.size(); ++n) {
		ReadNGrams(f, n, counts[n-1], vocab_, middle_vec_[n-2]);
		if (print_status) std::cerr << "Loaded " << n << "-grams" << std::endl;
	}
	ReadNGrams(f, counts.size(), counts[counts.size() - 1], vocab_, longest_);
	if (print_status) std::cerr << "Loading complete" << std::endl;
}

namespace {

float SumBackoffs(const float *begin, const float *end) {
	float ret = 0.0;
	for (; begin != end; ++begin) {
		ret += *begin;
	}
	return ret;
}

} // namespace

/* Ugly optimized function.
 * in_state contains the previous ngram's length and backoff probabilites to
 * be used here.  out_state is populated with the found ngram length and
 * backoffs that the next call will find useful.  
 *
 * The search goes in increasing order of ngram length.  
 */
float Model::InternalIncrementalScore(
		const State &in_state,
		const uint32_t *const words_begin,
		const uint32_t *const words_end,
		State &out_state) const {
	assert(words_end > words_begin);

	// This is end pointer passed to SumBackoffs.
	const detail::ProbBackoff &unigram = unigram_[*words_begin];
	if (*words_begin == vocab_.NotFound()) {
		out_state.ngram_length_ = 0;
		// all of backoff.
		return unigram.prob + SumBackoffs(
				in_state.backoff_.data(),
				in_state.backoff_.data() + std::min<unsigned int>(in_state.NGramLength(), order_ - 1));
	}
	boost::array<float, kMaxOrder - 1>::iterator backoff_out(out_state.backoff_.begin());
	*backoff_out = unigram.backoff;
	if (in_state.NGramLength() == 0) {
		out_state.ngram_length_ = 1;
		// No backoff because NGramLength() == 0 and unknown can't have backoff.
		return unigram.prob;
	}
	++backoff_out;

	// Ok now we now that the bigram contains known words.  Start by looking it up.
	
	float prob = unigram.prob;
	uint64_t lookup_hash = static_cast<uint64_t>(*words_begin);
	const uint32_t *words_iter = words_begin + 1;
	std::vector<Middle>::const_iterator mid_iter(middle_vec_.begin());
	for (; ; ++mid_iter, ++words_iter, ++backoff_out) {
		if (words_iter == words_end) {
			// ran out of words, so there shouldn't be backoff.
			out_state.ngram_length_ = (words_iter - words_begin);
			return prob;
		}
		lookup_hash = detail::CombineWordHash(lookup_hash, *words_iter);
		if (mid_iter == middle_vec_.end()) break;
		Middle::const_iterator found(mid_iter->find(lookup_hash));
		if (found == mid_iter->end()) {
			// Found an ngram of length words_iter - words_begin, but not of length words_iter - words_begin + 1.
			// Sum up backoffs for histories of length
			//   [words_iter - words_begin, std::min(in_state.NGramLength(), order_ - 1)).
			// These correspond to
			//   &in_state.backoff_[words_iter - words_begin - 1] to ending_backoff
			// which is the same as
			//   &in_state.backoff_[(mid_iter - middle_vec_.begin())] to ending_backoff.
			out_state.ngram_length_ = (words_iter - words_begin);
			return prob + SumBackoffs(
					in_state.backoff_.data() + (mid_iter - middle_vec_.begin()), 
					in_state.backoff_.data() + std::min<unsigned int>(in_state.NGramLength(), order_ - 1));
		}
		*backoff_out = found->second.backoff;
		prob = found->second.prob;
	}
	
	// A (order_-1)-gram was found.  Look for order_-gram.
	Longest::const_iterator found(longest_.find(lookup_hash));
	if (found == longest_.end()) {
		// It's an (order_-1)-gram
		out_state.ngram_length_ = order_ - 1;
		return prob + in_state.backoff_[order_ - 1 - 1];
	}
	// It's an order_-gram
	out_state.ngram_length_ = order_;
	if (order_ < kMaxOrder) {
		// In this case, State hashing and equality will check ngram_length_ entries.
		// However, the last entry is not a valid backoff weight, so here it is set
		// to 0.0.  Specifically
		// order_ - 1 = min(out_state.ngram_length_, order_ - 1) = min(out_state.ngram_length_, kMaxOrder - 1) - 1 = out_state.ngram_length_ - 1
		out_state.backoff_[order_ - 1] = 0.0;
	}
	return found->second.prob;	
}

} // namespace ngram
} // namespace lm

#ifndef LM_NGRAM_H__
#define LM_NGRAM_H__

#include "lm/base.hh"
#include "util/numbers.hh"
#include "util/string_piece.hh"

#include <boost/array.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/unordered_map.hpp>
#include <boost/noncopyable.hpp>

#include <algorithm>
#include <vector>
#include <memory>

namespace lm {
namespace ngram {

namespace detail {
struct VocabularyFriend;
} // namespace detail

class Vocabulary : public base::Vocabulary {
	public:
	  Vocabulary() {}

		WordIndex Index(const std::string &str) const {
			return Index(StringPiece(str));
		}

		WordIndex Index(const StringPiece &str) const {
			boost::unordered_map<StringPiece, WordIndex>::const_iterator i(ids_.find(str));
			return (__builtin_expect(i == ids_.end(), 0)) ? not_found_ : i->second;
		}

		const char *Word(WordIndex index) const {
			return strings_[index].c_str();
		}

	protected:
		// friend interface for populating.
		friend struct detail::VocabularyFriend;

		void Reserve(size_t to) {
			strings_.reserve(to);
			ids_.rehash(to + 1);
		}

		WordIndex InsertUnique(std::string *word) {
			std::pair<boost::unordered_map<StringPiece, WordIndex>::const_iterator, bool> res(ids_.insert(std::make_pair(StringPiece(*word), available_)));
			if (__builtin_expect(!res.second, 0)) {
				delete word;
				throw WordDuplicateVocabLoadException(*word, res.first->second, available_);
			}
			strings_.push_back(word);
			return available_++;
		}

		void FinishedLoading() {
			SetSpecial(Index(StringPiece("<s>")), Index(StringPiece("</s>")), Index(StringPiece("<unk>")), available_);
		}

	private:
		// TODO: optimize memory use here by using one giant buffer, preferably premade by a binary file format.
		boost::ptr_vector<std::string> strings_;
		boost::unordered_map<StringPiece, WordIndex> ids_;
};

namespace detail {

struct IdentityHash : public std::unary_function<uint64_t, size_t> {
  size_t operator()(const uint64_t key) const { return key; }
};
struct Prob {
	Prob(float in_prob) : prob(in_prob) {}
	Prob() {}
	float prob;
};
struct ProbBackoff : Prob {
	ProbBackoff(float in_prob, float in_backoff) : Prob(in_prob), backoff(in_backoff) {}
	ProbBackoff() {}
	float backoff;
};

} // namespace detail

// Should return the same results as SRI
class Model : boost::noncopyable {
	private:
		// If you need more than 5 change this and recompile.
		// Having this limit means that State can be
		// (kMaxOrder - 1) * sizeof(float) bytes instead of
		// sizeof(float*) + (kMaxOrder - 1) * sizeof(float) + malloc overhead
		static const std::size_t kMaxOrder = 5;

	public:
		class State {
			public:
				State() {}

				// Faster copying using only the valid entries.
			  State(const State &other) : ngram_length_(other.ngram_length_) {
					CopyValid(other);
				}

				State &operator=(const State &other) {
					ngram_length_ = other.ngram_length_;
					CopyValid(other);
					return *this;
				}

				bool operator==(const State &other) const {
					if (ngram_length_ != other.ngram_length_) return false;
					for (const float *first = backoff_.data(), *second = other.backoff_.data();
							first != backoff_.data() + ValidLength(); ++first, ++second) {
						// No arithmetic was performed on these values, so an exact comparison is justified.
						if (*first != *second) return false;
					}
					return true;
				}

				unsigned int NGramLength() const { return ngram_length_; }

			private:
				friend class Model;
				friend size_t hash_value(const State &state);

				size_t ValidLength() const {
				  return std::min<unsigned int>(ngram_length_, kMaxOrder - 1);
				}

				void CopyValid(const State &other) {
					const float *from;
					float *to;
					for (from = other.backoff_.data(), to = backoff_.data();
							to != backoff_.data() + ValidLength(); ++from, ++to) {
						*to = *from;
					}
				}

				unsigned int ngram_length_;

				// The first min(ngram_length_, Model::order_ - 1) entries are valid backoff weights.
				// backoff_[0] is the backoff for unigrams.
				// The first min(ngram_length_, kMaxOrder - 1) entries must be copied and
				// may be used for hashing or equality.   
				boost::array<float, kMaxOrder - 1> backoff_;
		};

		explicit Model(const char *arpa, bool status = false);

		const Vocabulary &GetVocabulary() const { return vocab_; }

		State BeginSentenceState() const {
			State ret;
			ret.backoff_[0] = begin_sentence_backoff_;
			ret.ngram_length_ = 1;
			return ret;
		}

		template <class ReverseHistoryIterator> LogDouble IncrementalScore(
			  const State &in_state,
			  const ReverseHistoryIterator &hist_begin,
			  const ReverseHistoryIterator &hist_end,
			  const WordIndex new_word,
			  State &out_state) const {
			const unsigned int words_len = std::min<unsigned int>(in_state.NGramLength() + 1, order_);
			uint32_t words[words_len];
			uint32_t *const words_end = words + words_len;
			words[0] = new_word;
			uint32_t *dest = words + 1;
			ReverseHistoryIterator src(hist_begin);
			for (; dest != words_end; ++dest, ++src) {
				if (src == hist_end) {
					for (; dest != words_end; ++dest) {
						*dest = vocab_.BeginSentence();
					}
					break;
				}
				*dest = *src;
			}
			// words is in reverse order.
			return LogDouble(AlreadyLogTag(), InternalIncrementalScore(in_state, words, words_end, out_state));
		}

		size_t Order() const { return order_; }

	private: 
		float InternalIncrementalScore(
				const State &in_state,
				const uint32_t *const words,
				const uint32_t *const words_end,
				State &out_state) const;

		size_t order_;

		Vocabulary vocab_;

		// Cached unigram_[vocab_.BeginSentence()].backoff
		float begin_sentence_backoff_;

		typedef std::vector<detail::ProbBackoff> Unigram;
		Unigram unigram_;
		
		typedef boost::unordered_map<uint64_t, detail::ProbBackoff, detail::IdentityHash> Middle;
		std::vector<Middle> middle_vec_; 
		
		typedef boost::unordered_map<uint64_t, detail::Prob, detail::IdentityHash> Longest;
		Longest longest_;
};

inline size_t hash_value(const Model::State &state) {
	size_t ret = 0;
	boost::hash_combine(ret, state.ngram_length_);
	for (const float *val = state.backoff_.data(); val != state.backoff_.data() + state.ValidLength(); ++val) {
		boost::hash_combine(ret, *val);
	}
	return ret;
}

// This just owns Model, which in turn owns Vocabulary.  Only reason this class
// exists is to provide the same interface as the other models.
class Owner : boost::noncopyable {
	public:
		explicit Owner(const char *file_name) : model_(file_name) {}

		const Vocabulary &GetVocabulary() const { return model_.GetVocabulary(); }

		const Model &GetModel() const { return model_; }

	private:
		const Model model_;
};

} // namespace ngram
} // namespace lm

#endif // LM_NGRAM_H__

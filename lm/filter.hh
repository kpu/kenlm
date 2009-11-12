#ifndef LM_FILTER_H__
#define LM_FILTER_H__
/* Filter an ARPA language model to only contain words found in a vocabulary
 * plus <s>, </s>, and <unk>.
 */

#include "util/multi_intersection.hh"
#include "util/string_piece.hh"
#include "util/tokenize_piece.hh"

#include <boost/lexical_cast.hpp>
#include <boost/noncopyable.hpp>
#include <boost/progress.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/unordered/unordered_map.hpp>
#include <boost/unordered/unordered_set.hpp>

#include <fstream>
#include <istream>
#include <memory>
#include <string>
#include <vector>

#include <err.h>
#include <string.h>

namespace lm {

void WriteCounts(std::ostream &out, const std::vector<size_t> &number);
size_t SizeNeededForCounts(const std::vector<size_t> &number);
void ReadCounts(std::istream &in, std::vector<size_t> &number);

void ReadNGramHeader(std::istream &in_lm, unsigned int length);
void ReadEnd(std::istream &in_lm);

// Writes an ARPA file.  This has to be seekable so the counts can be written
// at the end.  Hence, I just have it own a std::fstream instead of accepting
// a separately held std::ostream.  
class OutputLM {
  public:
    explicit OutputLM(const char *name);

		void ReserveForCounts(std::streampos reserve);

    void BeginLength(unsigned int length);

    inline void AddNGram(const std::string &line) {
      file_ << line << '\n';
      ++fast_counter_;
    }

    void EndLength(unsigned int length);

    void Finish();

  private:
    std::fstream file_;
    size_t fast_counter_;
    std::vector<size_t> counts_;
};

inline bool IsTag(const StringPiece &value) {
	// The parser should never give an empty string.
	assert(!value.empty());
	return (value.data()[0] == '<' && value.data()[value.size() - 1] == '>');
}

class SingleOutputFilter : boost::noncopyable {
	public:
		void ReserveForCounts(std::streampos reserve) {
			out_.ReserveForCounts(reserve);
		}

		void BeginLength(unsigned int length) {
			out_.BeginLength(length);
		}

		void EndLength(unsigned int length) {
			out_.EndLength(length);
		}

		void Finish() {
			out_.Finish();
		}

	protected:
		explicit SingleOutputFilter(const char *out) : out_(out) {}

		OutputLM out_;
};

class SingleVocabFilter : public SingleOutputFilter {
	public:
		SingleVocabFilter(std::istream &vocab, const char *out);

		template <class Iterator> void AddNGram(unsigned int length, const Iterator &begin, const Iterator &end, const std::string &line) {
			for (Iterator i = begin; i != end; ++i) {
				if (IsTag(*i)) continue;
				if (words_.find(*i) == words_.end()) return;
			}
			out_.AddNGram(line);
		}

	private:
		// Keep this order so backing_ is deleted after words_.
		boost::ptr_vector<std::string> backing_;

		boost::unordered_set<StringPiece> words_;
};

class MultipleVocabSingleOutputFilter : public SingleOutputFilter {
	public:
		typedef boost::unordered_map<StringPiece, std::vector<unsigned int> > Map;

		MultipleVocabSingleOutputFilter(const Map &vocabs, const char *out) : SingleOutputFilter(out), vocabs_(vocabs) {}

		template <class Iterator> void AddNGram(unsigned int length, const Iterator &begin, const Iterator &end, const std::string &line) {
			std::vector<boost::iterator_range<const unsigned int*> > sets;
			sets.reserve(length);

			Map::const_iterator found;
			for (Iterator i(begin); i != end; ++i) {
				if (IsTag(*i)) continue;
				if (vocabs_.end() == (found = vocabs_.find(*i))) return;
				sets.push_back(boost::iterator_range<const unsigned int*>(&*found->second.begin(), &*found->second.end()));
			}
			if (sets.empty() || util::FirstIntersection(sets)) {
				out_.AddNGram(line);
			}
		}

	private:
		const Map &vocabs_;
};

class MultipleVocabMultipleOutputFilter {
	public:
		typedef boost::unordered_map<StringPiece, std::vector<unsigned int> > Map;

		MultipleVocabMultipleOutputFilter(const Map &vocabs, unsigned int sentence_count, const char *prefix);

		void ReserveForCounts(std::streampos reserve) {
			for (boost::ptr_vector<OutputLM>::iterator i = files_.begin(); i != files_.end(); ++i) {
				i->ReserveForCounts(reserve);
			}
		}

		void BeginLength(unsigned int length) {
			for (boost::ptr_vector<OutputLM>::iterator i = files_.begin(); i != files_.end(); ++i) {
				i->BeginLength(length);
			}
		}

		// Callback from AllIntersection that does AddNGram.
		class Callback {
			public:
				Callback(boost::ptr_vector<OutputLM> &files, const std::string &line) : files_(files), line_(line) {}

				void operator()(unsigned int index) {
					files_[index].AddNGram(line_);
				}

			private:
				boost::ptr_vector<OutputLM> &files_;
				const std::string &line_;
		};

		template <class Iterator> void AddNGram(unsigned int length, const Iterator &begin, const Iterator &end, const std::string &line) {
			std::vector<boost::iterator_range<const unsigned int*> > sets;
			sets.reserve(length);

			Map::const_iterator found;
			for (Iterator i(begin); i != end; ++i) {
				if (IsTag(*i)) continue;
				if (vocabs_.end() == (found = vocabs_.find(*i))) return;
				sets.push_back(boost::iterator_range<const unsigned int*>(&*found->second.begin(), &*found->second.end()));
			}
			if (sets.empty()) {
				for (boost::ptr_vector<OutputLM>::iterator i = files_.begin(); i != files_.end(); ++i) {
					i->AddNGram(line);
				}
				return;
			}
			
			Callback cb(files_, line);
			util::AllIntersection(sets, cb);
		}

		void EndLength(unsigned int length) {
			for (boost::ptr_vector<OutputLM>::iterator i = files_.begin(); i != files_.end(); ++i) {
				i->EndLength(length);
			}
		}

		void Finish() {
			for (boost::ptr_vector<OutputLM>::iterator i = files_.begin(); i != files_.end(); ++i) {
				i->Finish();
			}
		}

	private:
		boost::ptr_vector<OutputLM> files_;
		const Map &vocabs_;
};

class PrepareMultipleVocab : boost::noncopyable {
	public:
		typedef boost::unordered_map<StringPiece, std::vector<unsigned int> > Vocabs;

		PrepareMultipleVocab() : temp_str_(new std::string) {
			to_insert_.second.push_back(0);
		}

		std::string &TempStr() {
			return *temp_str_;
		}

		void Insert() {
			to_insert_.first = StringPiece(*temp_str_);
			std::pair<Vocabs::iterator,bool> table(vocabs_.insert(to_insert_));
			if (table.second) {
				storage_.push_back(temp_str_);
				temp_str_.reset(new std::string());
			} else {
        if (table.first->second.back() != to_insert_.second.front()) {
  				table.first->second.push_back(to_insert_.second.front());
        }
			}
		}

		void EndSentence() {
			++MutableSentenceIndex();
		}

		// The PrepareMultipleVocab must still exist while this is used.
		const Vocabs &GetVocabs() const {
			return vocabs_;
		}

		unsigned int SentenceCount() const {
			return to_insert_.second.front();
		}

	private:
		unsigned int &MutableSentenceIndex() {
			return to_insert_.second.front();
		}
		boost::ptr_vector<std::string> storage_;
		Vocabs vocabs_;
		std::pair<StringPiece, std::vector<unsigned int> > to_insert_;
		std::auto_ptr<std::string> temp_str_;
};

template <class Filter> void FilterNGrams(std::istream &in, unsigned int length, size_t number, Filter &to) {
	std::string line;
  ReadNGramHeader(in, length);
  to.BeginLength(length);
  boost::progress_display display(number, std::cerr, std::string("Length ") + boost::lexical_cast<std::string>(length) + ": " + boost::lexical_cast<std::string>(number) + " total\n");
	for (unsigned int i = 0; i < number; ++i) {
    ++display;
		if (!std::getline(in, line))
			err(2, "Reading ngram failed.  Maybe the counts are wrong?");

		util::PieceIterator<'\t'> tabber(line);
		if (!tabber) 
			errx(3, "Empty \"%s\"", line.c_str());
		if (!++tabber)
			errx(3, "No tab in line \"%s\"", line.c_str());

		to.AddNGram(length, util::PieceIterator<' '>(*tabber), util::PieceIterator<' '>::end(), line);
	}
	if (!getline(in, line)) err(2, "Reading from input lm");
	if (!line.empty()) errx(3, "Expected blank line after ngrams");
	to.EndLength(length);
}

template <class Filter> void FilterARPA(std::istream &in_lm, Filter &to) {
	std::vector<size_t> number;
	ReadCounts(in_lm, number);
	to.ReserveForCounts(SizeNeededForCounts(number));
	for (unsigned int i = 0; i < number.size(); ++i) {
		FilterNGrams(in_lm, i + 1, number[i], to);
	}
	ReadEnd(in_lm);
  to.Finish();
}

} // namespace lm

#endif // LM_FILTER_H__

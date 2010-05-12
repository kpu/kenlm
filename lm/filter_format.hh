#ifndef LM_FILTER_FORMAT_H__
#define LM_FITLER_FORMAT_H__

#include "lm/arpa_io.hh"
#include "lm/count_io.hh"

#include <boost/ptr_container/ptr_vector.hpp>

#include <iosfwd>

namespace lm {

template <class Single> class MultipleOutput {
  private:
    typedef boost::ptr_vector<Single> Singles;
    typedef typename Singles::iterator SinglesIterator;

  public:
    MultipleOutput(const char *prefix, size_t number) {
      files_.reserve(number);
      std::string tmp;
      for (unsigned int i = 0; i < number; ++i) {
        tmp = prefix;
        tmp += boost::lexical_cast<std::string>(i);
        files_.push_back(new Single(tmp.c_str()));
      }
    }

    void AddNGram(const std::string &line) {
      for (SinglesIterator i = files_.begin(); i != files_.end(); ++i)
        i->AddNGram(line);
    }

    template <class Iterator> void AddNGram(const Iterator &begin, const Iterator &end, const std::string &line) {
      for (SinglesIterator i = files_.begin(); i != files_.end(); ++i)
        i->AddNGram(begin, end, line);
    }

    void SingleAddNGram(size_t offset, const std::string &line) {
      files_[offset].AddNGram(line);
    }

    template <class Iterator> void SingleAddNGram(size_t offset, const Iterator &begin, const Iterator &end, const std::string &line) {
      files_[offset].AddNGram(begin, end, line);
    }

  protected:
    Singles files_;
};

class MultipleARPAOutput : public MultipleOutput<ARPAOutput> {
  public:
    MultipleARPAOutput(const char *prefix, size_t number) : MultipleOutput<ARPAOutput>(prefix, number) {}

    void ReserveForCounts(std::streampos reserve) {
      for (boost::ptr_vector<ARPAOutput>::iterator i = files_.begin(); i != files_.end(); ++i)
        i->ReserveForCounts(reserve);
    }

    void BeginLength(unsigned int length) {
      for (boost::ptr_vector<ARPAOutput>::iterator i = files_.begin(); i != files_.end(); ++i)
        i->BeginLength(length);
    }

    void EndLength(unsigned int length) {
      for (boost::ptr_vector<ARPAOutput>::iterator i = files_.begin(); i != files_.end(); ++i)
        i->EndLength(length);
    }

    void Finish() {
      for (boost::ptr_vector<ARPAOutput>::iterator i = files_.begin(); i != files_.end(); ++i)
        i->Finish();
    }
};

template <class Filter> class DispatchARPAInput {
  public:
    explicit DispatchARPAInput(Filter &filter) : filter_(filter), output_(filter_.GetOutput()) {}

    void ReserveForCounts(std::streampos reserve) { output_.ReserveForCounts(reserve); }
    void BeginLength(unsigned int length) { output_.BeginLength(length); }

    template <class Iterator> void AddNGram(const Iterator &begin, const Iterator &end, const std::string      &line) {
      filter_.AddNGram(begin, end, line);
    }

    void EndLength(unsigned int length) { output_.EndLength(length); }
    void Finish() { output_.Finish(); }

  private:
    Filter &filter_;
    typename Filter::Output &output_;
};

struct ARPAFormat {
  typedef ARPAOutput Output;
  typedef MultipleARPAOutput Multiple;
  static void Copy(std::istream &in, Output &out) {
    ReadARPA(in, out);
  }
  template <class Filter> static void RunFilter(std::istream &in, Filter &filter) {
    DispatchARPAInput<Filter> dispatcher(filter);
    ReadARPA(in, dispatcher);
  }
};

struct CountFormat {
  typedef CountOutput Output;
  typedef MultipleOutput<Output> Multiple;
  static void Copy(std::istream &in, Output &out) {
    ReadCount(in, out);
  }
  template <class Filter> static void RunFilter(std::istream &in, Filter &filter) {
    ReadCount(in, filter);
  }
};

} // namespace lm

#endif // LM_FILTER_FORMAT_H__

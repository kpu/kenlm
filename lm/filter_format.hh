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

template <class Filter, class Output> class DispatchInput {
  public:
    DispatchInput(Filter &filter, Output &output) : filter_(filter), output_(output) {}

    template <class Iterator> void AddNGram(const Iterator &begin, const Iterator &end, const std::string &line) {
      filter_.AddNGram(begin, end, line, output_);
    }

  protected:
    Filter &filter_;
    Output &output_;
};

template <class Filter, class Output> class DispatchARPAInput : public DispatchInput<Filter, Output> {
  private:
    typedef DispatchInput<Filter, Output> B;

  public:
    DispatchARPAInput(Filter &filter, Output &output) : B(filter, output) {}

    void ReserveForCounts(std::streampos reserve) { B::output_.ReserveForCounts(reserve); }
    void BeginLength(unsigned int length) { B::output_.BeginLength(length); }

    void EndLength(unsigned int length) {
      B::filter_.Flush();
      B::output_.EndLength(length);
    }
    void Finish() { B::output_.Finish(); }
};

struct ARPAFormat {
  typedef ARPAOutput Output;
  typedef MultipleARPAOutput Multiple;
  static void Copy(std::istream &in, Output &out) {
    ReadARPA(in, out);
  }
  template <class Filter, class Out> static void RunFilter(std::istream &in, Filter &filter, Out &output) {
    DispatchARPAInput<Filter, Out> dispatcher(filter, output);
    ReadARPA(in, dispatcher);
  }
};

struct CountFormat {
  typedef CountOutput Output;
  typedef MultipleOutput<Output> Multiple;
  static void Copy(std::istream &in, Output &out) {
    ReadCount(in, out);
  }
  template <class Filter, class Out> static void RunFilter(std::istream &in, Filter &filter, Out &output) {
    DispatchInput<Filter, Out> dispatcher(filter, output);
    ReadCount(in, dispatcher);
  }
};

} // namespace lm

#endif // LM_FILTER_FORMAT_H__

#ifndef LM_ARPA_FILTER_H__
#define LM_ARPA_FITLER_H__

#include "lm/arpa_io.hh"

#include <boost/ptr_container/ptr_vector.hpp>

#include <iosfwd>

namespace lm {

class MultipleARPAOutput {
  public:
    MultipleARPAOutput(const char *prefix, size_t number);

    void ReserveForCounts(std::streampos reserve) {
      for (boost::ptr_vector<ARPAOutput>::iterator i = files_.begin(); i != files_.end(); ++i)
        i->ReserveForCounts(reserve);
    }

    void BeginLength(unsigned int length) {
      for (boost::ptr_vector<ARPAOutput>::iterator i = files_.begin(); i != files_.end(); ++i)
        i->BeginLength(length);
    }

    void AddNGram(const std::string &line) {
      for (boost::ptr_vector<ARPAOutput>::iterator i = files_.begin(); i != files_.end(); ++i)
        i->AddNGram(line);
    }

    template <class Iterator> void AddNGram(unsigned int length, const Iterator &begin, const Iterator &end, const std::string      &line) {
      for (boost::ptr_vector<ARPAOutput>::iterator i = files_.begin(); i != files_.end(); ++i)
        i->AddNGram(length, begin, end, line);
    }

    void SingleAddNGram(size_t offset, const std::string &line) {
      files_[offset].AddNGram(line);
    }

    template <class Iterator> void SingleAddNGram(size_t offset, unsigned int length, const Iterator &begin, const Iterator &end,   const std::string &line) {
      files_[offset].AddNGram(length, begin, end, line);
    }

    void EndLength(unsigned int length) {
      for (boost::ptr_vector<ARPAOutput>::iterator i = files_.begin(); i != files_.end(); ++i)
        i->EndLength(length);
    }

    void Finish() {
      for (boost::ptr_vector<ARPAOutput>::iterator i = files_.begin(); i != files_.end(); ++i)
        i->Finish();
    }

  private:
    boost::ptr_vector<ARPAOutput> files_;
};

template <class Filter> class DispatchARPAInput {
  public:
    explicit DispatchARPAInput(Filter &filter) : filter_(filter), output_(filter_.GetOutput()) {}

    void ReserveForCounts(std::streampos reserve) { output_.ReserveForCounts(reserve); }
    void BeginLength(unsigned int length) { output_.BeginLength(length); }

    template <class Iterator> void AddNGram(unsigned int length, const Iterator &begin, const Iterator &end, const std::string      &line) {
      filter_.AddNGram(length, begin, end, line);
    }

    void EndLength(unsigned int length) { output_.EndLength(length); }
    void Finish() { output_.Finish(); }

  private:
    Filter &filter_;
    typename Filter::Output &output_;
};

} // namespace lm

#endif // LM_ARPA_FILTER_H__

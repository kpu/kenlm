#ifndef BUILDER_MULTI_FILE_STREAM__
#define BUILDER_MULTI_FILE_STREAM__

#include "builder/ngram.hh"

#include <boost/lexical_cast.hpp>
#include <tpie/file_stream.h>

#include <string>

namespace lm {
namespace builder {

template <unsigned N, template <unsigned> class Gram>
class MultiFileStream;

template <unsigned N, template <unsigned> class Gram>
class MultiFileStream {
  public:
    typedef tpie::file_stream<Gram<N> > Head;
    typedef MultiFileStream<N - 1, Gram> Tail;

    explicit MultiFileStream(const std::string &base, tpie::access_type accessType = tpie::access_read_write)
      : tail(base, accessType) {
        std::string name(base + boost::lexical_cast<std::string>(N));
        head.open(name.c_str(), accessType);
      }

    Head &GetHead() { return head_; }

    Tail &GetTail() { return tail_; }

  private:
    Head head_;
    Tail tail;
};

template <template <unsigned K> class Gram>
struct MultiFileStream<0, Gram> {

  MultiFileStream(const char**, tpie::access_type)
  {}
};

}
}

#endif // BUILDER_MULTI_FS_STREAM__


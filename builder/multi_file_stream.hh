#ifndef BUILDER_MULTI_FILE_STREAM__
#define BUILDER_MULTI_FILE_STREAM__

#include "builder/ngram.hh"

#include <tpie/file_stream.h>

namespace lm {
namespace builder {

template <unsigned N, template <unsigned K> class Gram>
struct MultiFileStream {
  typedef MultiFileStream< N, Gram > This;
  typedef MultiFileStream< N - 1, Gram > That;

  tpie::file_stream< Gram<N> > head;
  That tail;

  MultiFileStream(const char** filenames, tpie::access_type accessType = tpie::access_read_write)
    : tail(filenames + 1, accessType)
  {
    head.open(*filenames, accessType);
  }
};

template <template <unsigned K> class Gram >
struct MultiFileStream< 0, Gram > {
  typedef MultiFileStream< 0, Gram > This;

  MultiFileStream(const char**, tpie::access_type)
  { }
};

}
}

#endif // BUILDER_MULTI_FS_STREAM__


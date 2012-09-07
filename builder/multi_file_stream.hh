#ifndef BUILDER_MULTI_FILE_STREAM__
#define BUILDER_MULTI_FILE_STREAM__

#include "builder/ngram.hh"

#include <tpie/file_stream.h>

namespace lm {
namespace builder {

template <unsigned N, template <unsigned> class Gram>
class MultiFileStream;

namespace {

template < bool ScrewedUp, unsigned I, unsigned N, template <unsigned> class Gram >
struct getter {
  static inline tpie::file_stream< Gram<I> >* get(MultiFileStream<N, Gram>& mfs) {
    return getter< (I > N), I, N - 1, Gram>::get(mfs.tail);
  }
};

template < unsigned I, template <unsigned> class Gram >
struct getter<false, I, I, Gram> {
  static inline tpie::file_stream< Gram<I> >* get(MultiFileStream<I, Gram>& mfs) {
    return &mfs.head;
  }
};

template < unsigned I, unsigned N, template <unsigned> class Gram >
struct getter<true, I, N, Gram> {
  static inline tpie::file_stream< Gram<I> >* get(MultiFileStream<N, Gram>& mfs) {
    std::terminate(); // Unreachable.
    return NULL;
  }
};

}

template <unsigned N, template <unsigned> class Gram>
class MultiFileStream {
  typedef MultiFileStream< N, Gram > This;
  typedef tpie::file_stream< Gram<N> > Head;
  typedef MultiFileStream< N - 1, Gram > Tail;

  template <bool, unsigned, unsigned, template <unsigned> class> friend struct getter;

  Head head;
  Tail tail;

public:
  MultiFileStream(const char** filenames, tpie::access_type accessType = tpie::access_read_write)
    : tail(filenames + 1, accessType)
  {
    head.open(*filenames, accessType);
  }

  template <unsigned I>
  tpie::file_stream< Gram<I> >* get() {
    return lm::builder::getter< (I > N), I, N, Gram>::get(*this);
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


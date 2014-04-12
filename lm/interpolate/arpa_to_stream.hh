#include "lm/read_arpa.hh"
#include "util/file_piece.hh"

#include <vector>

#include <stdint.h>

namespace util { namespace stream { class ChainPositions; } }

namespace lm {

namespace ngram { class GrowableVocab; }

namespace interpolate {

class ARPAToStream {
  public:
    // Takes ownership of fd.
    explicit ARPAToStream(int fd, ngram::GrowableVocab &vocab);

    std::size_t Order() const { return counts_.size(); }

    const std::vector<uint64_t> &Counts() const { return counts_; }

    void Run(const util::stream::ChainPositions &positions);

  private:
    util::FilePiece in_;

    std::vector<uint64_t> counts_;

    ngram::GrowableVocab &vocab_;
};

}} // namespaces

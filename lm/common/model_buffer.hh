#ifndef LM_COMMON_MODEL_BUFFER_H
#define LM_COMMON_MODEL_BUFFER_H

/* Format with separate files in suffix order.  Each file contains
 * n-grams of the same order.
 */

#include "util/file.hh"
#include "util/fixed_array.hh"

#include <string>
#include <vector>

namespace util { namespace stream { class Chains; } }

namespace lm {

class ModelBuffer {
  public:
    // Construct for writing.
    ModelBuffer(const std::string &file_base, bool keep_buffer, bool output_q, const std::vector<uint64_t> &counts);

    // Load from file.
    explicit ModelBuffer(const std::string &file_base);

    void Sink(util::stream::Chains &chains);

    void Source(util::stream::Chains &chains);

    // The order of the n-gram model that is associated with the model buffer.
    std::size_t Order() const { return counts_.size(); }
    const std::vector<uint64_t> &Counts() const { return counts_; }

  private:
    const std::string file_base_;
    const bool keep_buffer_;
    bool output_q_;
    std::vector<uint64_t> counts_;

    util::FixedArray<util::scoped_fd> files_;
};

} // namespace lm

#endif // LM_COMMON_MODEL_BUFFER_H

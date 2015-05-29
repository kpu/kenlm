#ifndef LM_INTERPOLATE_MERGE_VOCAB_H
#define LM_INTERPOLATE_MERGE_VOCAB_H

#include "util/file.hh"
#include "util/fixed_array.hh"

namespace lm {
namespace interpolate {

class UniversalVocab;

// Takes ownership of vocab_files
void MergeVocab(util::FixedArray<util::scoped_fd> &vocab_files, UniversalVocab &vocab, int vocab_write_file);

}} // namespaces

#endif // LM_INTERPOLATE_MERGE_VOCAB_H

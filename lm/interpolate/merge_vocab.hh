#ifndef LM_INTERPOLATE_MERGE_VOCAB_H
#define LM_INTERPOLATE_MERGE_VOCAB_H

#include "util/file.hh"
#include "util/fixed_array.hh"

namespace lm {
namespace interpolate {

class UniversalVocab;

// Takes ownership of vocab_files
void MergeVocabIndex(util::FixedArray<util::scoped_fd> &vocab_files, UniversalVocab &vocab);

}} // namespaces

#endif // LM_INTERPOLATE_MERGE_VOCAB_H

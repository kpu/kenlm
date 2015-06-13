#ifndef LM_INTERPOLATE_MERGE_VOCAB_H
#define LM_INTERPOLATE_MERGE_VOCAB_H

#include "lm/word_index.hh"
#include "util/file.hh"
#include "util/fixed_array.hh"

namespace lm {

class EnumerateVocab;

namespace interpolate {

class UniversalVocab;

// Takes ownership of vocab_files.
// The combined vocabulary is enumerated with enumerate.
// Returns the size of the combined vocabulary.
WordIndex MergeVocab(util::FixedArray<util::scoped_fd> &vocab_files, UniversalVocab &vocab, EnumerateVocab &enumerate);

}} // namespaces

#endif // LM_INTERPOLATE_MERGE_VOCAB_H

#include "lm/virtual_interface.hh"

#include "lm/exception.hh"

namespace lm {
namespace base {

Vocabulary::~Vocabulary() {}

void Vocabulary::SetSpecial(WordIndex begin_sentence, WordIndex end_sentence, WordIndex not_found, WordIndex available) {
  begin_sentence_ = begin_sentence;
  end_sentence_ = end_sentence;
  not_found_ = not_found;
  available_ = available;
  if (begin_sentence_ == not_found_) throw BeginSentenceMissingException();
  if (end_sentence_ == not_found_) throw EndSentenceMissingException();
}

Model::~Model() {}

} // namespace base
} // namespace lm

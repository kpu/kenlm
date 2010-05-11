#include "lm/arpa_filter.hh"

namespace lm {

MultipleARPAOutput::MultipleARPAOutput(const char *prefix, size_t number) {
  files_.reserve(number);
  std::string tmp;
  for (unsigned int i = 0; i < number; ++i) {
    tmp = prefix;
    tmp += boost::lexical_cast<std::string>(i);
    files_.push_back(new ARPAOutput(tmp.c_str()));
  }
}
 
} // namespace lm

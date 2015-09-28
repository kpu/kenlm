#include "lm/common/model_buffer.hh"
#include "util/exception.hh"
#include "util/file_stream.hh"
#include "util/file.hh"
#include "util/file_piece.hh"
#include "util/stream/io.hh"
#include "util/stream/multi_stream.hh"

#include <boost/lexical_cast.hpp>

namespace lm {

namespace {
const char kMetadataHeader[] = "KenLM intermediate binary file";
} // namespace

ModelBuffer::ModelBuffer(StringPiece file_base, bool keep_buffer, bool output_q)
  : file_base_(file_base.data(), file_base.size()), keep_buffer_(keep_buffer), output_q_(output_q),
    vocab_file_(keep_buffer ? util::CreateOrThrow((file_base_ + ".vocab").c_str()) : util::MakeTemp(file_base_)) {}
  
ModelBuffer::ModelBuffer(StringPiece file_base)
  : file_base_(file_base.data(), file_base.size()), keep_buffer_(false) {
  const std::string full_name = file_base_ + ".kenlm_intermediate";
  util::FilePiece in(full_name.c_str());
  StringPiece token = in.ReadLine();
  UTIL_THROW_IF2(token != kMetadataHeader, "File " << full_name << " begins with \"" << token << "\" not " << kMetadataHeader);

  token = in.ReadDelimited();
  UTIL_THROW_IF2(token != "Counts", "Expected Counts, got \"" << token << "\" in " << full_name);
  char got;
  while ((got = in.get()) == ' ') {
    counts_.push_back(in.ReadULong());
  }
  UTIL_THROW_IF2(got != '\n', "Expected newline at end of counts.");

  token = in.ReadDelimited();
  UTIL_THROW_IF2(token != "Payload", "Expected Payload, got \"" << token << "\" in " << full_name);
  token = in.ReadDelimited();
  if (token == "q") {
    output_q_ = true;
  } else if (token == "pb") {
    output_q_ = false;
  } else {
    UTIL_THROW(util::Exception, "Unknown payload " << token);
  }

  vocab_file_.reset(util::OpenReadOrThrow((file_base_ + ".vocab").c_str()));

  files_.Init(counts_.size());
  for (unsigned long i = 0; i < counts_.size(); ++i) {
    files_.push_back(util::OpenReadOrThrow((file_base_ + '.' + boost::lexical_cast<std::string>(i + 1)).c_str()));
  }
}

void ModelBuffer::Sink(util::stream::Chains &chains, const std::vector<uint64_t> &counts) {
  counts_ = counts;
  // Open files.
  files_.Init(chains.size());
  for (std::size_t i = 0; i < chains.size(); ++i) {
    if (keep_buffer_) {
      files_.push_back(util::CreateOrThrow(
            (file_base_ + '.' + boost::lexical_cast<std::string>(i + 1)).c_str()
            ));
    } else {
      files_.push_back(util::MakeTemp(file_base_));
    }
    chains[i] >> util::stream::Write(files_.back().get());
  }
  if (keep_buffer_) {
    util::scoped_fd metadata(util::CreateOrThrow((file_base_ + ".kenlm_intermediate").c_str()));
    util::FileStream meta(metadata.get(), 200);
    meta << kMetadataHeader << "\nCounts";
    for (std::vector<uint64_t>::const_iterator i = counts_.begin(); i != counts_.end(); ++i) {
      meta << ' ' << *i;
    }
    meta << "\nPayload " << (output_q_ ? "q" : "pb") << '\n';
  }
}

void ModelBuffer::Source(util::stream::Chains &chains) {
  assert(chains.size() <= files_.size());
  for (unsigned int i = 0; i < chains.size(); ++i) {
    chains[i] >> util::stream::PRead(files_[i].get());
  }
}

void ModelBuffer::Source(std::size_t order_minus_1, util::stream::Chain &chain) {
  chain >> util::stream::PRead(files_[order_minus_1].get());
}

} // namespace

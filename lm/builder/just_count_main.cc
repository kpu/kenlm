#include "lm/builder/corpus_count.hh"
#include "lm/builder/sort.hh"
#include "util/stream/chain.hh"
#include "util/stream/io.hh"
#include "util/stream/sort.hh"
#include "util/file.hh"
#include "util/file_piece.hh"
#include "util/usage.hh"

#include <boost/program_options.hpp>

#include <string>

namespace {
class SizeNotify {
  public:
    SizeNotify(std::size_t &out) : behind_(out) {}

    void operator()(const std::string &from) {
      behind_ = util::ParseSize(from);
    }

  private:
    std::size_t &behind_;
};

boost::program_options::typed_value<std::string> *SizeOption(std::size_t &to, const char *default_value) {
  return boost::program_options::value<std::string>()->notifier(SizeNotify(to))->default_value(default_value);
}

} // namespace

int main(int argc, char *argv[]) {
  namespace po = boost::program_options;
  unsigned order;
  std::size_t ram;
  std::string temp_prefix, vocab;
  po::options_description options("corpus count");
  options.add_options()
    ("order,o", po::value<unsigned>(&order)->required(), "Order")
    ("temp_prefix,T", po::value<std::string>(&temp_prefix)->default_value("/tmp"), "Temporary file prefix")
    ("memory,S", SizeOption(ram, "80%"), "RAM")
    ("vocab", po::value<std::string>(&vocab)->required(), "Vocab mapping to use");
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, options), vm);
  po::notify(vm);

  util::NormalizeTempPrefix(temp_prefix);

  util::scoped_fd vocab_file(util::OpenReadOrThrow(vocab.c_str()));

  std::size_t blocks = 1;

  std::size_t memory_for_chain =
    // This much memory to work with after vocab hash table.
    static_cast<float>(ram - util::SizeOrThrow(vocab_file.get())) /
    // Solve for block size including the dedupe multiplier for one block.
    (static_cast<float>(blocks) + lm::builder::CorpusCount::DedupeMultiplier(order)) *
    // Chain likes memory expressed in terms of total memory.
    static_cast<float>(blocks);
  
  util::stream::Chain chain(util::stream::ChainConfig(lm::builder::NGram::TotalSize(order), blocks, memory_for_chain));
  util::FilePiece f(0, NULL, &std::cerr);
  uint64_t token_count = 0;
  lm::WordIndex type_count = 0;
  lm::builder::CorpusCount counter(f, vocab_file.get(), token_count, type_count, chain.BlockSize() / chain.EntrySize(), lm::THROW_UP, false);
  chain >> boost::ref(counter);

  util::stream::SortConfig sort_config;
  sort_config.temp_prefix = temp_prefix;
  sort_config.buffer_size = 64 * 1024 * 1024;
  sort_config.total_memory = ram;
  // Inefficiently copies if there's only one block.
  util::stream::BlockingSort(chain, sort_config, lm::builder::SuffixOrder(order), lm::builder::AddCombiner());
  chain >> util::stream::WriteAndRecycle(1);
}

#include "lm/interpolate/backoff_reunification.hh"
#include "lm/word_index.hh"
#include "util/stream/io.hh"
#include "lm/builder/model_buffer.hh"

int main() {
  using namespace lm::interpolate;

  const std::string prob_file = "csorted-ngrams";
  const std::string boff_file = "backoffs";

  ReunifyConfig config;
  config.sort_config.temp_prefix = "/tmp/";
  config.sort_config.buffer_size = 1 << 26;  // 64MB
  config.sort_config.total_memory = 1 << 30; // 1GB
  config.file_base = "interpolated";
  config.num_blocks = 2;
  config.max_ram = 1 << 30; // 1GB

  lm::builder::ModelBuffer in_prob_buf(prob_file);
  lm::builder::ModelBuffer in_boff_buf(boff_file);
  util::stream::Chains prob_chains(in_prob_buf.Order());
  util::stream::Chains backoff_chains(in_prob_buf.Order());

  for (std::size_t i = 0; i < in_prob_buf.Order(); ++i) {
    prob_chains.push_back(util::stream::ChainConfig(
        sizeof(lm::WordIndex) * (i + 1) + sizeof(float), config.num_blocks,
        config.max_ram));

    backoff_chains.push_back(util::stream::ChainConfig(
        sizeof(float), config.num_blocks, config.max_ram));
  }

  in_prob_buf.Source(prob_chains);
  in_boff_buf.Source(backoff_chains);

  ReunifyBackoff(config, prob_chains, backoff_chains);

  return 0;
}

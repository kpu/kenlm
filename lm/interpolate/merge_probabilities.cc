#include "lm/interpolate/merge_probabilities.hh"
#include "lm/common/ngram_stream.hh"
#include "lm/interpolate/bounded_sequence_encoding.hh"
#include "lm/interpolate/interpolate_info.hh"

#include <algorithm>

namespace lm {
namespace interpolate {

/**
 * Helper to generate the BoundedSequenceEncoding used for writing the
 * from values.
 */
BoundedSequenceEncoding MakeEncoder(const InterpolateInfo &info, uint8_t order) {
  util::FixedArray<uint8_t> max_orders(info.orders.size());
  for (std::size_t i = 0; i < info.orders.size(); ++i) {
    max_orders.push_back(std::min(order, info.orders[i]));
  }
  return BoundedSequenceEncoding(max_orders.begin(), max_orders.end());
}

namespace {
/**
 * A simple wrapper class that holds information needed to read and write
 * the ngrams of a particular order. This class has the memory needed to
 * buffer the data needed for the recursive process of computing the
 * probabilities and "from" values for each component model.
 *
 * "From" values indicate, for each model, what order (as an index, so -1)
 * was backed off to in order to arrive at a probability. For example, if a
 * 5-gram model (order index 4) backed off twice, we would write a 2.
 */
class NGramHandler {
public:
  NGramHandler(uint8_t order, const InterpolateInfo &ifo,
               util::FixedArray<util::stream::ChainPositions> &models_by_order)
      : info(ifo),
        encoder(MakeEncoder(info, order)),
        out_record(order, encoder.EncodedLength()) {

    // have to init outside since NGramStreams doesn't forward to
    // GenericStreams ctor given a ChainPositions
    inputs.Init(models_by_order[order - 1]);

    probs.Init(info.Models());
    from.Init(info.Models());
  }


  /**
   * @return the input stream for a particular model that corresponds to
   * this ngram order
   */
  lm::NGramStream<ProbBackoff> &operator[](std::size_t idx) {
    return inputs[idx];
  }

  const InterpolateInfo &info;
  lm::NGramStreams<ProbBackoff> inputs;
  BoundedSequenceEncoding encoder;
  PartialProbGamma out_record;
  util::FixedArray<float> probs;
  util::FixedArray<uint8_t> from;
};

/**
 * A collection of NGramHandlers.
 */
class NGramHandlers : public util::FixedArray<NGramHandler> {
public:
  explicit NGramHandlers(std::size_t num)
      : util::FixedArray<NGramHandler>(num) {
  }

  void push_back(
      std::size_t order, const InterpolateInfo &info,
      util::FixedArray<util::stream::ChainPositions> &models_by_order) {
    new (end()) NGramHandler(order, info, models_by_order);
    Constructed();
  }
};

/**
 * The recursive helper function that computes probability and "from"
 * values for all ngrams matching a particular suffix.
 *
 * The current order can be computed as the suffix length + 1. Note that
 * the suffix could be empty (suffix_begin == suffix_end == NULL), in which
 * case we are handling unigrams with the UNK token as the fallback
 * probability.
 *
 * @param handlers The full collection of handlers
 * @param suffix_begin A start iterator for the suffix
 * @param suffix_end An end iterator for the suffix
 * @param fallback_probs The probabilities of this ngram if we need to
 *  back off (that is, the probability of the suffix)
 * @param fallback_from The order that the corresponding fallback
 *  probability in the fallback_probs is from
 * @param combined_fallback interpolated fallback_probs
 * @param outputs The output streams, one for each order
 */
void HandleSuffix(NGramHandlers &handlers, WordIndex *suffix_begin,
                  WordIndex *suffix_end,
                  const util::FixedArray<float> &fallback_probs,
                  const util::FixedArray<uint8_t> &fallback_from,
                  float combined_fallback,
                  util::stream::Streams &outputs) {
  uint8_t order = std::distance(suffix_begin, suffix_end) + 1;
  if (order >= outputs.size()) return;

  util::stream::Stream &output = outputs[order - 1];
  NGramHandler &handler = handlers[order - 1];

  while (true) {
    // find the next smallest ngram which matches our suffix
    WordIndex *minimum = NULL;
    for (std::size_t i = 0; i < handler.info.Models(); ++i) {
      if (!std::equal(suffix_begin, suffix_end, handler[i]->begin() + 1))
        continue;

      // if we either haven't set a minimum yet or this one is smaller than
      // the minimum we found before, replace it
      WordIndex *last = handler[i]->begin();
      if (!minimum || *last < *minimum) { minimum = handler[i]->begin(); }
    }

    // no more ngrams of this order match our suffix, so we're done
    // the check against the max int here is how we know that the streams
    // are done being read
    if (!minimum || *minimum == std::numeric_limits<WordIndex>::max()) return;

    handler.out_record.ReBase(output.Get());
    std::copy(minimum, minimum + order, handler.out_record.begin());

    // "multiply" together probabilities from all models, populating the from
    // field as we go for each model
    handler.out_record.Prob() = 0;
    for (std::size_t i = 0; i < handler.info.Models(); ++i) {
      // found this ngram without backing off, so record its probability
      // and order - 1 directly
      if (std::equal(handler.out_record.begin(), handler.out_record.end(),
                     handler[i]->begin())) {
        handler.probs[i] = handler.info.lambdas[i] * handler[i]->Value().prob;
        handler.from[i] = order - 1;

        // consume the ngram
        ++handler[i];
      }
      // otherwise, we needed to back off, so grab the probability value from
      // the fallback probabilities and record the fallback level
      else {
        handler.probs[i] = fallback_probs[i];
        handler.from[i] = fallback_from[i];
      }

      handler.out_record.Prob() += handler.probs[i];
    }
    handler.out_record.LowerProb() = combined_fallback;
    handler.encoder.Encode(handler.from.begin(),
                           handler.out_record.FromBegin());

    // we've handled this particular ngram, so now recurse to the higher
    // order using the current ngram as the suffix
    HandleSuffix(handlers, handler.out_record.begin(), handler.out_record.end(),
                 handler.probs, handler.from, handler.out_record.Prob(), outputs);
    // consume the output
    ++output;
  }
}

/**
 * Kicks off the recursion for computing the probabilities and "from"
 * values for each ngram order. We begin by handling the UNK token that
 * should be at the front of each of the unigram input streams. This is
 * then output to the stream and it is used as the fallback for handling
 * our unigram case, the unigram used as the fallback for the bigram case,
 * etc.
 */
void HandleNGrams(NGramHandlers &handlers, util::stream::Streams &outputs) {
  PartialProbGamma unk_record(1, 0);
  // First: populate the unk probabilities by reading the first unigram
  // from each stream
  util::FixedArray<float> unk_probs(handlers[0].info.Models());

  // start by populating the ngram id from the first stream
  lm::NGram<ProbBackoff> ngram = *handlers[0][0];
  unk_record.ReBase(outputs[0].Get());
  std::copy(ngram.begin(), ngram.end(), unk_record.begin());
  unk_record.Prob() = 0;

  // then populate the probabilities into unk_probs while "multiply" the
  // model probabilities together into the unk record
  //
  // note that from doesn't need to be set for unigrams
  for (std::size_t i = 0; i < handlers[0].info.Models(); ++i) {
    ngram = *handlers[0][i];
    unk_probs[i] = ngram.Value().prob;
    unk_record.Prob() += handlers[0].info.lambdas[i] * unk_probs[i];
    assert(*ngram.begin() == kUNK);
    ++handlers[0][i];
  }
  float unk_combined = unk_record.Prob();
  unk_record.LowerProb() = unk_combined;
  // flush the unk output record
  ++outputs[0];

  // Then, begin outputting everything in lexicographic order: first we'll
  // get the unigram then the first bigram with that context, then the
  // first trigram with that bigram context, etc., until we exhaust all of
  // the ngrams, then all of the (n-1)grams, etc.
  //
  // This function is the "root" of this recursive process.
  util::FixedArray<uint8_t> unk_from(handlers[0].info.Models());
  for (std::size_t i = 0; i < handlers[0].info.Models(); ++i) {
    unk_from.push_back(0);
  }

  // the two nulls are to encode that our "fallback" word is the "0-gram"
  // case, e.g. we "backed off" to UNK
  // TODO: stop generating vocab ids and LowerProb for unigrams.
  HandleSuffix(handlers, NULL, NULL, unk_probs, unk_from, unk_combined, outputs);

  // Read the dummy "end-of-stream" symbol for each of the inputs
  for (std::size_t i = 0; i < handlers.size(); ++i) {
    for (std::size_t j = 0; j < handlers[i].inputs.size(); ++j) {
      UTIL_THROW_IF2(*handlers[i][j]->begin()
                         != std::numeric_limits<WordIndex>::max(),
                     "MergeProbabilities did not exhaust all ngram streams");
      ++handlers[i][j];
    }
  }
}
}

void MergeProbabilities(
    const InterpolateInfo &info,
    util::FixedArray<util::stream::ChainPositions> &models_by_order,
    util::stream::Chains &output_chains) {
  NGramHandlers handlers(models_by_order.size());
  for (std::size_t i = 0; i < models_by_order.size(); ++i) {
    handlers.push_back(i + 1, info, models_by_order);
  }

  util::stream::ChainPositions output_pos(output_chains);
  util::stream::Streams outputs(output_pos);

  HandleNGrams(handlers, outputs);
}
}
}

#include "lm/interpolate/backoff_and_normalize.hh"

#include "lm/common/compare.hh"
#include "lm/common/ngram_stream.hh"
#include "lm/interpolate/interpolate_info.hh"
#include "lm/weights.hh"
#include "lm/word_index.hh"
#include "util/scoped.hh"
#include "util/fixed_array.hh"

#include <functional>
#include <queue>

namespace lm { namespace interpolate {
namespace {

class BackoffMatrix {
  public:
    BackoffMatrix(std::size_t num_models, std::size_t max_order)
      : max_order_(max_order), backing_(num_models * max_order) {
      std::fill(backing_.begin(), backing_.end(), 0.0f);
    }

    float &Backoff(std::size_t model, std::size_t order_minus_1) {
      return backing_[model * max_order_ + order_minus_1];
    }

    float Backoff(std::size_t model, std::size_t order_minus_1) const {
      return backing_[model * max_order_ + order_minus_1];
    }

  private:
    const std::size_t max_order_;
    util::FixedArray<float> backing_;
};

struct SuffixLexicographicLess : public std::binary_function<NGramHeader, NGramHeader, bool> {
  bool operator()(const NGramHeader first, const NGramHeader second) const {
    for (const WordIndex *f = first.end() - 1, *s = second.end() - 1; f >= first.begin() && s >= second.begin(); --f, --s) {
      if (*f < *s) return true;
      if (*f > *s) return false;
    }
    return first.size() < second.size();
  }
};

class BackoffQueueEntry {
  public:
    BackoffQueueEntry(float &entry, const util::stream::ChainPosition &position)
      : entry_(entry), stream_(position) {
      entry_ = 0.0;
    }

    operator bool() const { return stream_; }

    NGramHeader operator*() const { return *stream_; }
    const NGramHeader *operator->() const { return &*stream_; }

    void Enter() {
      entry_ = stream_->Value().backoff;
    }

    BackoffQueueEntry &Next() {
      entry_ = 0.0;
      ++stream_;
      return *this;
    }

  private:
    float &entry_;
    NGramStream<ProbBackoff> stream_;
};

struct PtrGreater : public std::binary_function<const BackoffQueueEntry *, const BackoffQueueEntry *, bool> {
  bool operator()(const BackoffQueueEntry *first, const BackoffQueueEntry *second) const {
    return SuffixLexicographicLess()(**second, **first);
  }
};

class EntryOwner : public util::FixedArray<BackoffQueueEntry> {
  public:
    void push_back(float &entry, const util::stream::ChainPosition &position) {
      new (end()) BackoffQueueEntry(entry, position);
      Constructed();
    }
};

std::size_t MaxOrder(const util::FixedArray<util::stream::ChainPositions> &model) {
  std::size_t ret = 0;
  for (const util::stream::ChainPositions *m = model.begin(); m != model.end(); ++m) {
    ret = std::max(ret, m->size());
  }
  return ret;
}

class BackoffManager {
  public:
    BackoffManager(const util::FixedArray<util::stream::ChainPositions> &models)
      : entered_(MaxOrder(models)), matrix_(models.size(), entered_.size()) {
      std::size_t total = 0;
      for (const util::stream::ChainPositions *m = models.begin(); m != models.end(); ++m) {
        total += m->size();
      }
      for (std::size_t i = 0; i < entered_.size(); ++i) {
        entered_.push_back(models.size());
      }
      owner_.Init(total);
      for (const util::stream::ChainPositions *m = models.begin(); m != models.end(); ++m) {
        for (const util::stream::ChainPosition *j = m->begin(); j != m->end(); ++j) {
          owner_.push_back(matrix_.Backoff(m - models.begin(), j - m->begin()), *j);
          if (owner_.back()) {
            queue_.push(&owner_.back());
          }
        }
      }
    }

    // Move up the backoffs for the given n-gram.  The n-grams must be provided
    // in suffix lexicographic order.
    void Enter(NGramHeader to) {
      // Check that we exited properly.
      for (std::size_t i = to.Order() - 1; i < entered_.size(); ++i) {
        assert(entered_[i].empty());
      }
      SuffixLexicographicLess less;
      while (!queue_.empty() && less(**queue_.top(), to)) {
        BackoffQueueEntry *top = queue_.top();
        queue_.pop();
        if (top->Next())
          queue_.push(top);
      }
      while (!queue_.empty() && (*queue_.top())->Order() == to.Order() && std::equal(to.begin(), to.end(), (*queue_.top())->begin())) {
        BackoffQueueEntry *matches = queue_.top();
        entered_[to.Order() - 1].push_back(matches);
        matches->Enter();
        queue_.pop();
      }
    }

    void Exit(std::size_t order) {
      for (BackoffQueueEntry **i = entered_[order - 1].begin(); i != entered_[order - 1].end(); ++i) {
        if ((*i)->Next())
          queue_.push(*i);
      }
      entered_[order - 1].clear();
    }

    float Get(std::size_t model, std::size_t order_minus_1) const {
      return matrix_.Backoff(model, order_minus_1);
    }

  private:
    EntryOwner owner_;
    std::priority_queue<BackoffQueueEntry*, std::vector<BackoffQueueEntry*>, PtrGreater> queue_;

    // Indexed by order then just all the matching models.
    util::FixedArray<util::FixedArray<BackoffQueueEntry*> > entered_;

    std::size_t order_;

    BackoffMatrix matrix_;
};

} // namespace

}} // namespaces

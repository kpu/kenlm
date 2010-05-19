#include "util/tokenize_piece.hh"
#include "util/pcqueue.hh"

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/thread.hpp>

#include <fstream>
#include <functional>
#include <iostream>
#include <queue>

#include <assert.h>

const size_t kBatchBytes = 8192;
const size_t kBatchBuffer = 3;

struct LineItem {
  StringPiece ngrams;
  StringPiece count;
};

struct Batch {
  std::string buffer;
  std::vector<LineItem> lines;
};

bool ReadBatch(std::istream &file, Batch &batch) {
  // Read in buffer, with a pad for characters to the next newline.
  batch.buffer.clear();
  batch.buffer.reserve(kBatchBytes + 200);
  batch.buffer.resize(kBatchBytes);
  file.read(&batch.buffer[0], kBatchBytes);
  batch.buffer.resize(file.gcount());
  // Pad to next newline.
  char got;
  while (file.get(got) && got != '\n') {
    batch.buffer.push_back(got);
  }

  batch.lines.clear();
  LineItem item;
  for (util::PieceIterator<'\n'> l(batch.buffer); l; ++l) {
    util::PieceIterator<'\t'> t(*l);
    if (!t) {
      throw std::runtime_error("Empty line");
    }
    item.ngrams = *t;
    if (!++t) {
      throw std::runtime_error("No count");
    }
    item.count = *t;
    batch.lines.push_back(item);
  }

  return file;
}

class Reader : boost::noncopyable {
  public:
    Reader(const char *name, util::PCQueue<Batch*> &incoming) : file_(name), incoming_(incoming), outgoing_(kBatchBuffer) {
      decomp_.push(boost::iostreams::gzip_decompressor());
      decomp_.push(file_);
      thread_ = boost::thread(boost::ref(*this)).move();
    }

    void operator()() {
      Batch *batch;
      bool ret;
      do {
        incoming_.Consume(batch);
        ret = ReadBatch(decomp_, *batch);
        outgoing_.Produce(batch);
      } while (ret);
      outgoing_.Produce(NULL);
    }

    util::PCQueue<Batch*> &Incoming() { return incoming_; }
    util::PCQueue<Batch*> &Outgoing() { return outgoing_; }

  private:
    std::ifstream file_;
    boost::iostreams::filtering_istream decomp_;

    util::PCQueue<Batch*> &incoming_;
    util::PCQueue<Batch*> outgoing_;

    boost::thread thread_;
};

// Not a real iterator (it isn't copyable), but close
class FileIterator : boost::noncopyable {
  public:
    explicit FileIterator(const char *name, util::PCQueue<Batch*> &incoming) : reader_(name, incoming) {
      AdvanceBatch();
    }

    const LineItem &operator*() const {
      return *line_;
    }
    const LineItem *operator->() const {
      return &*line_;
    }

    FileIterator &operator++() {
      assert(batch_);
      if (++line_ == batch_->lines.end()) {
        reader_.Incoming().Produce(batch_);
        AdvanceBatch();
      }
      return *this;
    }

    operator bool() const {
      return batch_ != NULL;
    }

  private:
    void AdvanceBatch() {
      batch_ = reader_.Outgoing().Consume();
      if (batch_) line_ = batch_->lines.begin();
    }

    Reader reader_;

    Batch *batch_;

    std::vector<LineItem>::const_iterator line_;
};

struct FileIteratorNgramLess : public std::binary_function<const FileIterator *, const FileIterator *, bool> {
  bool operator()(const FileIterator *left, const FileIterator *right) const {
    return (*left)->ngrams < (*right)->ngrams;
  }
};

class Joiner {
  public:
    Joiner(const char **names_begin, const char **names_end) : batches_((names_end - names_begin) * kBatchBuffer), recycle_(batches_.size()) {
      for (std::vector<Batch>::iterator i = batches_.begin(); i != batches_.end(); ++i) {
        recycle_.Produce(&*i);
      }
      for (const char **i = names_begin; i != names_end; ++i) {
        iterators_.push_back(new FileIterator(*i, recycle_));
        queue_.push(&iterators_.back());
      }
    }

    void Run(std::ostream &out) {
      FileIteratorNgramLess less;
      while (!queue_.empty()) {
        FileIterator *top = queue_.top();
        queue_.pop();
        out << (*top)->ngrams << '\t';
        // Unique.
        if (less(top, queue_.top())) {
          out << (*top)->count;
        } else {
          unsigned long long sum = boost::lexical_cast<unsigned long long>((*top)->count);
          while (!queue_.empty()) {
            FileIterator *same = queue_.top();
            if (less(top, same)) break;
            sum += boost::lexical_cast<unsigned long long>((*same)->count);
            queue_.pop();
            if (++*same) queue_.push(same);
          }
          out << sum;
        }
        out << '\n';
        if (++*top) queue_.push(top);
      }
    }

  private:
    std::vector<Batch> batches_;

    util::PCQueue<Batch*> recycle_;

    boost::ptr_vector<FileIterator> iterators_;

    std::priority_queue<FileIterator*, std::vector<FileIterator*>, FileIteratorNgramLess> queue_;
};

int main(int argc, const char *argv[]) {
  Joiner join(argv + 1, argv + argc);
  join.Run(std::cout);
}

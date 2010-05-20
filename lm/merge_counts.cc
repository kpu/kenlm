#include "util/tokenize_piece.hh"
#include "util/pcqueue.hh"

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/thread.hpp>

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <queue>
#include <vector>

#include <assert.h>

const size_t kBatchBytes = 65536;
const size_t kBatchBuffer = 3;

struct LineItem {
  std::vector<StringPiece> tokenized;
  StringPiece ngrams;
  unsigned long long count;
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
    item.tokenized.clear();
    std::copy(util::PieceIterator<' '>(item.ngrams), util::PieceIterator<' '>::end(), std::back_insert_iterator<std::vector<StringPiece> >(item.tokenized));
    if (!++t) {
      throw std::runtime_error("No count");
    }
    item.count = boost::lexical_cast<unsigned long long>(*t);
    batch.lines.push_back(item);
  }

  if (!file && !file.eof()) throw std::runtime_error("File reading error");

  return file;
}

class Reader : boost::noncopyable {
  public:
    Reader(const char *name, util::PCQueue<Batch*> &incoming) : name_(name), file_(name), incoming_(incoming), outgoing_(kBatchBuffer) {
      decomp_.push(boost::iostreams::gzip_decompressor());
      decomp_.push(file_);
      thread_ = boost::thread(boost::ref(*this)).move();
    }

    void operator()() {
      Batch *batch;
      bool ret;
      do {
        incoming_.Consume(batch);
        try {
          ret = ReadBatch(decomp_, *batch);
        }
        catch(...) {
          std::cerr << "Caught error on " << name_ << std::endl;
          incoming_.Produce(batch);
          outgoing_.Produce(NULL);
          throw;
        }
        outgoing_.Produce(batch);
      } while (ret);
      outgoing_.Produce(NULL);
    }

    util::PCQueue<Batch*> &Incoming() { return incoming_; }
    util::PCQueue<Batch*> &Outgoing() { return outgoing_; }

  private:
    std::string name_;
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

struct FileIteratorNgramGreater : public std::binary_function<const FileIterator *, const FileIterator *, bool> {
  bool operator()(const FileIterator *left, const FileIterator *right) const {
    return (*left)->tokenized > (*right)->tokenized;
  }
};

void Join(const char **names_begin, const char **names_end, std::ostream &out) {
  std::vector<Batch> batches((names_end - names_begin) * kBatchBuffer * 2);
  util::PCQueue<Batch*> recycle(batches.size());
  for (std::vector<Batch>::iterator i = batches.begin(); i != batches.end(); ++i) {
    recycle.Produce(&*i);
  }

  boost::ptr_vector<FileIterator> iterators;
  std::priority_queue<FileIterator*, std::vector<FileIterator*>, FileIteratorNgramGreater> queue;
  for (const char **i = names_begin; i != names_end; ++i) {
    iterators.push_back(new FileIterator(*i, recycle));
    queue.push(&iterators.back());
  }

  while (!queue.empty()) {
    FileIterator *top = queue.top();
    queue.pop();
    unsigned long long sum = (*top)->count;
    while (!queue.empty()) {
      FileIterator *same = queue.top();
      if ((*same)->ngrams != (*top)->ngrams) break;
      sum += (*same)->count;
      queue.pop();
      if (++*same) queue.push(same);
    }
    out << (*top)->ngrams << '\t' << sum << '\n';
    if (++*top) queue.push(top);
  }
}

int main(int argc, const char *argv[]) {
  Join(argv + 1, argv + argc, std::cout);
}

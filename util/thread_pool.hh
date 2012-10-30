#ifndef UTIL_THREAD_POOL__
#define UTIL_THREAD_POOL__

#include "util/pcqueue.hh"

#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/optional.hpp>
#include <boost/thread.hpp>

#include <iostream>

#include <stdlib.h>

namespace util {

template <class HandlerT> class Worker : boost::noncopyable {
  public:
    typedef HandlerT Handler;
    typedef typename Handler::Request Request;

    template <class Construct> Worker(PCQueue<Request> &in, Construct &construct, Request &poison)
      : in_(in), handler_(construct), thread_(boost::ref(*this)), poison_(poison) {}

    // Only call from thread.
    void operator()() {
      Request request;
      while (1) {
        in_.Consume(request);
        if (request == poison_) return;
        try {
          (*handler_)(request);
        }
        catch(std::exception &e) {
          std::cerr << "Handler threw " << e.what() << std::endl;
          abort();
        }
        catch(...) {
          std::cerr << "Handler threw an exception, dropping request" << std::endl;
          abort();
        }
      }
    }

    void Join() {
      thread_.join();
    }

  private:
    PCQueue<Request> &in_;

    boost::optional<Handler> handler_;

    boost::thread thread_;

    Request poison_;
};

template <class HandlerT> class ThreadPool : boost::noncopyable {
  public:
    typedef HandlerT Handler;
    typedef typename Handler::Request Request;

    template <class Construct> ThreadPool(size_t queue_length, size_t workers, Construct handler_construct, Request poison) : in_(queue_length), poison_(poison) {
      for (size_t i = 0; i < workers; ++i) {
        workers_.push_back(new Worker<Handler>(in_, handler_construct, poison));
      }
    }

    ~ThreadPool() {
      for (size_t i = 0; i < workers_.size(); ++i) {
        Produce(poison_);
      }
      for (typename boost::ptr_vector<Worker<Handler> >::iterator i = workers_.begin(); i != workers_.end(); ++i) {
        i->Join();
      }
    }

    void Produce(const Request &request) {
      in_.Produce(request);
    }

    // For adding to the queue.
    PCQueue<Request> &In() { return in_; }

  private:
    PCQueue<Request> in_;

    boost::ptr_vector<Worker<Handler> > workers_;

    Request poison_;
};

} // namespace util

#endif // UTIL_THREAD_POOL__

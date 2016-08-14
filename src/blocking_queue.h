#ifndef __tealtree__BlockingQueue__
#define __tealtree__BlockingQueue__

#include <condition_variable>
#include <mutex>

#include "circular_buffer.h"

struct operation_aborted {};

template <class T>
class BlockingBoundedQueue {
public:
    typedef T value_type;
    BlockingBoundedQueue(size_t N) : q_(N), aborted_(false) {}
    void push(value_type data)
    {
        std::unique_lock<std::mutex> lk(mtx_);
        cv_pop_.wait(lk, [=]{ return !q_.full() || aborted_; });
        if (aborted_) throw operation_aborted();
        q_.push_back(std::move(data));
        cv_push_.notify_one();
    }
    value_type pop()
    {
        std::unique_lock<std::mutex> lk(mtx_);
        cv_push_.wait(lk, [=]{ return !q_.empty() || aborted_; });
        if (aborted_) throw operation_aborted();
        value_type result = std::move(q_.front());
        q_.pop_front();
        cv_pop_.notify_one();
        return result;
    }
    void abort()
    {
        std::lock_guard<std::mutex> lk(mtx_);
        aborted_ = true;
        cv_pop_.notify_all();
        cv_push_.notify_all();
    }
private:
    circular_buffer<value_type> q_;
    bool aborted_;
    std::mutex mtx_;
    std::condition_variable cv_push_;
    std::condition_variable cv_pop_;
};

#endif /* defined(__tealtree__BlockingQueue__) */

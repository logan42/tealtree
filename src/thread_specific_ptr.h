#ifndef __tealtree__THREAD_SPECIFIC_PTR__
#define __tealtree__THREAD_SPECIFIC_PTR__

#include <map>
#include <mutex>
#include <stdio.h>
#include <thread>

template <class T>
class thread_specific_ptr
{
private:
    std::map<std::thread::id, std::unique_ptr<T>> map;
    std::mutex mutex;
public:
    ~thread_specific_ptr()
    {
        std::unique_lock<std::mutex> lock(this->mutex);
        map.clear();
    }

    inline T * get()
    {
        std::unique_lock<std::mutex> lock(this->mutex);
        return this->map[std::this_thread::get_id()].get();
    }

    inline void reset(T * t)
    {
        std::unique_lock<std::mutex> lock(this->mutex);
        this->map[std::this_thread::get_id()] = std::unique_ptr<T>(t);
    }
};

#endif /* defined(__tealtree__THREAD_SPECIFIC_PTR__) */

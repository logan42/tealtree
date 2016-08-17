#ifndef __tealtree__circular_buffer__
#define __tealtree__circular_buffer__

#include <cassert>
#include <vector>

template<class T>
class circular_buffer
{
private:
    std::vector<T> data;
    size_t _size;
    size_t head, tail;
public:
    inline circular_buffer(size_t size)
        :data(size),
        _size(0),
        head(0),
        tail(0)
    {
        assert(size > 0);
    }

    inline bool empty()
    {
        bool result = _size == 0;
        if (result) {
            assert(head == tail);
        }
        return result;
    }

    inline bool full()
    {
        bool result = _size == data.size();
        if (result) {
            assert(head == tail);
        }
        return result;
    }

    inline T & front()
    {
        assert(!empty());
        return data[head];
    }

    inline void pop_front()
    {
        assert(!empty());
        head = (head + 1) % data.size();
        _size--;
    }

    inline void push_back(T && item)
    {
        assert(!full());
        data[tail] = std::move(item);
        tail = (tail + 1) % data.size();
        _size += 1;
    }
};

#endif /* defined(__tealtree__circular_buffer__) */

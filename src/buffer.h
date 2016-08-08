#ifndef __tealtree__BUFFER__
#define __tealtree__BUFFER__

#include <cassert>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>

struct Buffer
{
    static const size_t INITIAL_SIZE = 16;

    void * ptr;
    size_t size;
#ifndef NDEBUG
    size_t currently_used_by = 0;
    bool allocated = false;
#endif

    inline Buffer()
    {
        ptr = malloc(INITIAL_SIZE);
        size = INITIAL_SIZE;
    }

    inline ~Buffer()
    {
        assert(this->ptr != nullptr);
        assert(this->currently_used_by== 0);
        free(ptr);
    }

    inline void ensure(size_t required)
    {
        if (this->size >= required) {
            return;
        }
        size_t new_size = required;
        this->ptr = realloc(this->ptr, new_size);
        this->size = new_size;
    }
};

class AbstractBufferAllocator
{
protected:
    Buffer * buffer;

    inline AbstractBufferAllocator()
        :buffer(nullptr)
    {}

    inline AbstractBufferAllocator(Buffer * buffer)
        : buffer(buffer)
    {
#ifndef NDEBUG
        this->buffer->currently_used_by++;
#endif
    }

    inline AbstractBufferAllocator(const AbstractBufferAllocator & other)
        : buffer(other.buffer)
    {
#ifndef NDEBUG
        if (buffer != nullptr) {
            buffer->currently_used_by++;
        }
#endif
    }

    inline ~AbstractBufferAllocator()
    {
        if (this->buffer != nullptr) {
#ifndef NDEBUG
            assert(this->buffer->currently_used_by > 0);
            this->buffer->currently_used_by--;
#endif
        }
    }
};

template <class T>
class BufferAllocator : public AbstractBufferAllocator
{
public:
    typedef T value_type;

    inline BufferAllocator()
        :AbstractBufferAllocator()
    {}

    inline BufferAllocator(Buffer * buffer)
        : AbstractBufferAllocator(buffer)
    {}

    template <class U> 
    inline BufferAllocator(const BufferAllocator <U>& other)
        :AbstractBufferAllocator(other)
    {}

    inline T* allocate(std::size_t n)
    {
        bool do_alloc = false;
#if defined (_MSC_VER)
        //I hate you, MSVC.
        if (n == 1) {
            do_alloc = true;
        }
#endif
        if (do_alloc || (this->buffer == nullptr)) {
            return reinterpret_cast<T*>(malloc(sizeof(T) * n));
        }

#ifndef NDEBUG
        assert(!this->buffer->allocated);
        this->buffer->allocated = true;
#endif
        this->buffer->ensure(sizeof(T) * n);
        return reinterpret_cast<T*>(this->buffer->ptr);
    }

    inline void deallocate(T* p, std::size_t n)
    {
        //std::cout << "free(" << n << ")" << std::endl;
        bool do_free = false;
#if defined (_MSC_VER)
        if (n == 1) {
            do_free = true;
        }
#endif
        if (do_free || (this->buffer == nullptr)) {
            free(p);
            return;
        }

#ifndef NDEBUG
        assert(this->buffer->allocated);
        this->buffer->allocated = false;
#endif
    }
};

template <class T, class U>
bool operator==(const BufferAllocator <T>&, const BufferAllocator <U>&)
{
    return false;
}

template <class T, class U>
bool operator!=(const BufferAllocator <T>&, const BufferAllocator <U>&)
{
    return true;
}

#endif /* defined(__tealtree__BUFFER__) */

#ifndef __tealtree__COMPACT_VECTOR__
#define __tealtree__COMPACT_VECTOR__

#include <stdio.h>
#include <vector>

#include "buffer.h"
#include "md5.h"
#include "types.h"

template<const uint8_t V_BITS, class T>
class CompactSubByteVector
{
private:
    static const uint8_t T_BITS = 8 * sizeof(T);
    static const uint8_t VALUES_PER_T = T_BITS / V_BITS;
    static const uint8_t VALUES_PER_BYTE = 8 / V_BITS;
    static const uint8_t BIT_MASK = (1 << V_BITS) - 1;
    static const T T_BIT_MASK = ((T)BIT_MASK);
    typedef std::vector<T, BufferAllocator<T>> DATA_VECTOR_TYPE;
    DATA_VECTOR_TYPE data;
    T buffer;
    uint8_t buffer_size;
    DOC_ID _size;
#ifndef NDEBUG
    bool initial_filling_done = false;
#endif
public:
    typedef uint8_t ValueType;

    inline CompactSubByteVector() 
        : buffer(0), 
        buffer_size(0), 
        _size(0)
    {}

    inline CompactSubByteVector(Buffer * buffer, DOC_ID size)
        : data((T)0, 0, BufferAllocator<T>(buffer)),
        buffer(0),
        buffer_size(0),
        _size(size)
    {
        this->data.resize((size + VALUES_PER_T - 1) / VALUES_PER_T);
#ifndef NDEBUG
        initial_filling_done = true;
#endif
    }

    inline CompactSubByteVector(DOC_ID size, ValueType value) 
        : buffer(0), 
        buffer_size(0), 
        _size(size)
    {
        assert(value <= BIT_MASK);
        T pattern = 0;
        for (uint8_t i = 0; i < VALUES_PER_T; i++) {
            pattern |= value << (i * V_BITS);
        }
        data.resize((size + VALUES_PER_T - 1) / VALUES_PER_T, pattern);
#ifndef NDEBUG
        initial_filling_done = true;
#endif
    }


    inline DOC_ID size()
    {
        return _size;
    }

    inline void reserve(DOC_ID size)
    {
        data.reserve((size + VALUES_PER_T - 1) / VALUES_PER_T);
    }

    inline void push_back(ValueType v)
    {
        assert(v <= BIT_MASK);
        assert(!initial_filling_done);
        buffer |= ((T)v) << buffer_size;
        buffer_size += V_BITS;
        if (buffer_size >= T_BITS) {
            data.push_back(buffer);
            buffer = 0;
            buffer_size = 0;
        }
    }

    inline void push_back_flush()
    {
        assert(!initial_filling_done);
#ifndef NDEBUG
        initial_filling_done = true;
#endif
        _size = VALUES_PER_T * data.size() + buffer_size / V_BITS;
        if (buffer_size > 0) {
            data.push_back(buffer);
            buffer = 0;
            buffer_size = 0;
        }
    }

    inline ValueType operator[](DOC_ID index)
    {
        assert(initial_filling_done);
        assert(index < _size);
        //return (data[index / VALUES_PER_T] >> ((index % VALUES_PER_T) * V_BITS)) & BIT_MASK;
        return (data_as_bytes()[index / VALUES_PER_BYTE] >> ((index % VALUES_PER_BYTE) * V_BITS)) & BIT_MASK;
    }

    inline void set(DOC_ID index, ValueType value)
    {
        assert(index < _size);
        assert(value <= BIT_MASK);
        uint8_t & byte = data_as_bytes()[index / VALUES_PER_BYTE];
        uint8_t shift = (index % VALUES_PER_BYTE) * V_BITS;
        byte &= ~(BIT_MASK << shift);
        byte |= value << shift;
    }

    inline void invert()
    {
        for (size_t i = 0; i < data.size(); i++) {
            data[i] = ~data[i];
        }
    }

    class Iterator 
    {
    private:
        DATA_VECTOR_TYPE & data;
        DOC_ID ptr;
        T buffer;
        uint8_t buffer_count;
    public:
        inline Iterator(DATA_VECTOR_TYPE & data, DOC_ID start) 
            : data(data)
        {
            DOC_ID index = start / VALUES_PER_T;
            uint8_t offset= start % VALUES_PER_T;
            if (offset== 0) {
                ptr = index;
                buffer = 0;
                buffer_count = 0;
            }
            else {
                buffer = data[index];
                ptr = index + 1;
                buffer_count = VALUES_PER_T - offset;
                buffer >>= V_BITS * offset;
            }
        }
        inline ValueType next()
        {
            if (buffer_count == 0) {
                assert(ptr < data.size());
            buffer = data[ptr++];
            buffer_count = VALUES_PER_T;
}
ValueType result = (ValueType)(buffer & BIT_MASK);
buffer >>= V_BITS;
buffer_count--;
return result;
        }
    };

inline Iterator iterator(DOC_ID start = 0)
    {
        Iterator it(data, start);
        return it;
    }

class Writer
{
private:
    DATA_VECTOR_TYPE & data;
    DOC_ID ptr;
    T buffer;
    uint8_t buffer_size;
public:
    inline Writer(DATA_VECTOR_TYPE & data, DOC_ID start) : data(data)
    {
        DOC_ID index = start / VALUES_PER_T;
        uint8_t offset = start % VALUES_PER_T;
        ptr = index;
        if (offset == 0) {
            buffer = 0;
            buffer_size = 0;
        }
        else {
            buffer = data[index];
            buffer_size = V_BITS * offset;
            uint8_t bits_to_cleanup = T_BITS - buffer_size;
            // Cleaning up the higher (shift) bits
            buffer = erase_higher_bits(buffer, bits_to_cleanup);
        }
    }
    inline void write(ValueType v)
    {
        assert(v <= BIT_MASK);
        buffer |= ((T)v) << buffer_size;
        buffer_size += V_BITS;
        if (buffer_size >= T_BITS) {
            assert(ptr < data.size());
            data[ptr++] = buffer;
            buffer = 0;
            buffer_size = 0;
        }
    }
    void flush()
    {
        if (buffer_size == 0) {
            return;
        }
        assert(ptr < data.size());
        T current_value = data[ptr];
        // Clean up the lowest buffer_size  bits.
        current_value = erase_lower_bits(current_value, buffer_size);
        current_value |= buffer;
        data[ptr]= current_value; 
    }
    inline DOC_ID get_ptr()
    {
        return this->ptr * VALUES_PER_T + (this->buffer_size / V_BITS);
    }
};

inline Writer writer(DOC_ID start = 0)
{
    return Writer(data, start);
}

void copy(const CompactSubByteVector<V_BITS, T> & other_from, DOC_ID from, DOC_ID to, DOC_ID size)
{
    if (size == 0) {
        return;
    }
    DOC_ID from_index = from / VALUES_PER_T;
    DOC_ID to_index = to / VALUES_PER_T;
    DOC_ID from_end = from + size;
    DOC_ID to_end = to + size;
    //DOC_ID from_end_index = from_end / VALUES_PER_T;
    DOC_ID to_end_index = to_end / VALUES_PER_T;
    uint8_t from_shift = V_BITS * (from % VALUES_PER_T);
    uint8_t to_shift = V_BITS * (to % VALUES_PER_T);
    uint8_t to_end_shift = V_BITS * (to_end % VALUES_PER_T);
        DOC_ID from_last = from_end - 1;
        DOC_ID from_last_index = from_last / VALUES_PER_T;
        uint8_t from_last_shift = V_BITS * (from_last % VALUES_PER_T);

        // Step 0. If the entire range fits into a single element, treat this as a separate case.
    if (to_index == to_end_index) {
        T chunk;
        if (from_index == from_last_index) {
            uint8_t erase_lower = from_shift;
            uint8_t erase_higher = T_BITS - from_last_shift - V_BITS;
            chunk = other_from.data[from_index] << erase_higher >> erase_higher >> erase_lower;
        }
        else {
            assert(from_last_index == from_index + 1);
            uint8_t erase_lower = from_shift;
            uint8_t shift2 = T_BITS - erase_lower;
            uint8_t erase_higher = T_BITS - from_last_shift - V_BITS;
            chunk =
                (other_from.data[from_index] >> erase_lower)
                | (other_from.data[from_index + 1] << erase_higher >> erase_higher << shift2);
        }
        T to_chunk = this->data[to_index];
        uint8_t keep_lower = to_shift;
        uint8_t keep_higher = T_BITS - to_end_shift;
        to_chunk ^= erase_lower_bits(erase_higher_bits(to_chunk, keep_higher), keep_lower);
        to_chunk |= chunk << keep_lower;
        this->data[to_index] = to_chunk;
        return;
    }
    
    uint8_t buffer_lower, buffer_higher;
    T  buffer;
    DOC_ID from_ptr, to_ptr;
    // Step 1. Fill the first element.
    if (from_shift >= to_shift) {
        buffer_lower = from_shift - to_shift;
        buffer_higher = T_BITS - buffer_lower;
        buffer = lower_bits(this->data[to_index], to_shift);
        buffer |= higher_bits(other_from.data[from_index], T_BITS - from_shift) << to_shift;
        from_ptr = from_index + 1;
        to_ptr = to_index;
    }
    else {
        buffer_higher = to_shift - from_shift;
        buffer_lower = T_BITS - buffer_higher;
        T chunk =
            lower_bits(this->data[to_index], to_shift)
            | (higher_bits(other_from.data[from_index], T_BITS - from_shift) << to_shift);
        this->data[to_index] = chunk;
        buffer = higher_bits(other_from.data[from_index], buffer_higher);
        from_ptr = from_index + 1;
        to_ptr = to_index + 1;
    }

    // Step 2. Copy the middle part
    if (buffer_lower == 0) {
        this->data[to_ptr++] = buffer;
        DOC_ID copy_size = to_end_index - to_ptr;
        memcpy(&this->data[to_ptr], &other_from.data[from_ptr], sizeof(T) * copy_size);
        from_ptr += copy_size;
        to_ptr += copy_size;
        if (to_end_shift > 0) {
// There is one more element to be written. In the next step they will expect to find the value in the buffer.
            buffer = other_from.data[from_ptr++];
        }
    }
    else {
        for (; to_ptr < to_end_index; to_ptr++, from_ptr++) {
            T value = other_from.data[from_ptr];
            this->data[to_ptr] = buffer | (value << buffer_higher);
            buffer = value >> buffer_lower;
        }
    }

    //Step 3. Copy the tail
    if (buffer_higher < to_end_shift) {
        // We need to read one more element.
        buffer |= other_from.data[from_ptr++] << buffer_higher;
    }
    assert(from_ptr == from_last_index + 1);
    if (to_end_shift > 0) {
        T value = this->data[to_end_index];
        value = erase_lower_bits(value, to_end_shift) | lower_bits(buffer, to_end_shift);
        this->data[to_end_index] = value;
    }
    // Phew
}

std::string md5()
{
    if (this->_size == 0) {
        return "";
    }
    MD5 hash;
    hash.update(reinterpret_cast<char*>(&this->data[0]), sizeof(this->data[0]) * this->data.size());
    hash.finalize();
    return hash.hexdigest();
}

private:
    inline uint8_t * data_as_bytes()
    {
        return reinterpret_cast<uint8_t *>(&data[0]);
    }

    static inline T lower_bits(T value, uint8_t n_bits)
    {
        uint8_t shift = T_BITS - n_bits;
        return value << shift >> shift;
    }

    static inline T higher_bits(T value, uint8_t n_bits)
    {
        uint8_t shift = T_BITS - n_bits;
        return value >> shift;
    }

    static inline T erase_lower_bits(T value, uint8_t n_bits)
    {
        return value >> n_bits << n_bits;
    }

    static inline T erase_higher_bits(T value, uint8_t n_bits)
    {
        return value << n_bits >> n_bits;
    }

};

template<class T>
class CompactWholeByteVector
{
private:
    typedef std::vector<T, BufferAllocator<T>> DATA_VECTOR_TYPE;
    DATA_VECTOR_TYPE data;
public:
    typedef T ValueType;

    inline CompactWholeByteVector()
    {}

    inline CompactWholeByteVector(Buffer * buffer, DOC_ID size)
        : data((T)0, 0, BufferAllocator<T>(buffer))
    {
        this->data.resize(size);
    }

    inline DOC_ID size()
    {
        return data.size();
    }

    inline void reserve(DOC_ID size)
    {
        data.reserve(size);
    }

    inline void push_back(ValueType value)
    {
        data.push_back(value);
    }

    inline void push_back_flush()
    {
    }

    inline ValueType operator[](DOC_ID index)
    {
        if (index >= data.size()) {
            data.size();
        }
        assert(index < data.size());
        return data[index];
    }

    void copy(const CompactWholeByteVector<T> & other_from, DOC_ID from, DOC_ID to, DOC_ID size)
    {
        if (size == 0) {
            return;
        }
        memcpy(&this->data[to], &other_from.data[from], sizeof(T) * size);
    }

    std::string md5()
    {
        if (this->data.size() == 0) {
            return "";
        }
        MD5 hash;
        hash.update(reinterpret_cast<char*>(&this->data[0]), sizeof(this->data[0]) * this->data.size());
        hash.finalize();
        return hash.hexdigest();
    }

    class Iterator
    {
    private:
        DATA_VECTOR_TYPE & data;
        DOC_ID ptr;
    public:
        inline Iterator(DATA_VECTOR_TYPE & data, DOC_ID start) 
            : data(data),
            ptr(start)
        {}
        inline ValueType next()
        {
            assert(ptr < data.size());
            return data[ptr++];
        }
    };

    inline Iterator iterator(DOC_ID start = 0)
    {
        Iterator it(data, start);
        return it;
    }

    class Writer
    {
    private:
        DATA_VECTOR_TYPE & data;
        DOC_ID ptr;
    public:
        inline Writer(DATA_VECTOR_TYPE & data, DOC_ID start) 
            : data(data),
            ptr(start)
        {}
        inline void write(ValueType v)
        {
                assert(ptr < data.size());
                data[ptr++] = v;
        }
        inline void flush()
        {}
        inline DOC_ID get_ptr()
        {
            return ptr;
        }
    };

    inline Writer writer(DOC_ID start = 0)
    {
        return Writer(data, start);
    }

};


template<const uint8_t V_BITS>
class CompactVector;

template<> class CompactVector<1> : public CompactSubByteVector<1, uint64_t> {
public:
    typedef typename CompactSubByteVector<1, uint64_t>::ValueType ValueType;
    typedef typename CompactSubByteVector<1, uint64_t>::Iterator Iterator;
    inline CompactVector() : CompactSubByteVector() {};
    inline CompactVector(Buffer * buffer, DOC_ID size) : CompactSubByteVector(buffer, size) {};
    inline CompactVector(DOC_ID size, ValueType value) : CompactSubByteVector(size, value) {}
};
template<> class CompactVector<2> : public CompactSubByteVector<2, uint64_t> {
public:
    typedef typename CompactSubByteVector<2, uint64_t>::ValueType ValueType;
    typedef typename CompactSubByteVector<2, uint64_t>::Iterator Iterator;
    inline CompactVector() : CompactSubByteVector() {};
    inline CompactVector(Buffer * buffer, DOC_ID size) : CompactSubByteVector(buffer, size) {};
};
template<> class CompactVector<4> : public CompactSubByteVector<4, uint64_t> {
public:
    typedef typename CompactSubByteVector<4, uint64_t>::ValueType ValueType;
    typedef typename CompactSubByteVector<4, uint64_t>::Iterator Iterator;
    inline CompactVector() : CompactSubByteVector() {};
    inline CompactVector(Buffer * buffer, DOC_ID size) : CompactSubByteVector(buffer, size) {};
};

template<> class CompactVector<8> : public CompactWholeByteVector<uint8_t> {
public:
    typedef typename CompactWholeByteVector<uint8_t>::ValueType ValueType;
    typedef typename CompactWholeByteVector<uint8_t>::Iterator Iterator;
    inline CompactVector() : CompactWholeByteVector() {};
    inline CompactVector(Buffer * buffer, DOC_ID size) : CompactWholeByteVector(buffer, size) {};
};
template<> class CompactVector<16> : public CompactWholeByteVector<uint16_t> {
public:
    typedef typename CompactWholeByteVector<uint16_t>::ValueType ValueType;
    typedef typename CompactWholeByteVector<uint16_t>::Iterator Iterator;
    inline CompactVector() : CompactWholeByteVector() {};
    inline CompactVector(Buffer * buffer, DOC_ID size) : CompactWholeByteVector(buffer, size) {};
};


template<const uint8_t V_BITS>
class CompactVectorForSparseFeature : public CompactVector<V_BITS>
{
public:
    typedef typename CompactVector<V_BITS>::ValueType ValueType;
};

template<>
class CompactVectorForSparseFeature <1>
{
};

#endif /* defined(__tealtree__COMPACT_VECTOR__) */

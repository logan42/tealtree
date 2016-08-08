#ifndef __tealtree__VAR_INT_BUFFER__
#define __tealtree__VAR_INT_BUFFER__

#include <algorithm>
#include <assert.h>
#include <stdio.h>
#include <vector>

#include "md5.h"


template <typename T, typename B, typename S>
class VarIntBuffer
{
private:
    const static uint8_t B_BITS = 8 * sizeof(B);
    const static uint8_t B_SIGNIFICANT_BITS = B_BITS - 1;
    const static B MSB = ((B)1u) << B_SIGNIFICANT_BITS;
    const static B NOT_MSB = (B)~MSB;
    std::vector<B> data;
public:
    inline VarIntBuffer()
    {

    }

    inline S size() 
    {
        return (S)data.size();
    }

    inline void append_tail(S length)
    {
this->        data.resize(data.size() + length, (B)0);
    }

    inline void resize(S size)
    {
        this->data.resize(size);
    }

    inline void clear()
    {
        this->data.clear();
    }

    inline void move(S from, S to, S size)
    {
        memmove(&this->data[to], &this->data[from], sizeof(data[0]) * size);
    }

    inline void copy(const VarIntBuffer<T,B,S> & other_from, S from, S to, S size)
    {
        if (size == 0) {
            return;
        }
        memcpy(&this->data[to], &other_from.data[from], sizeof(data[0]) * size);
    }

    std::string md5(S begin, S size)
    {
        if (this->data.size() == 0) {
            return "";
        }
        MD5 hash;
        hash.update(reinterpret_cast<char*>(&this->data[begin]), sizeof(this->data[0]) * size);
        hash.finalize();
        return hash.hexdigest();

    }

    class Iterator
    {
    private:
        const std::vector<B> & data;
        S ptr;

        inline B read_byte()
        {
            assert(ptr < data.size());
            return data[ptr++];
        }
    public:
        inline Iterator(const std::vector<B> & data, S ptr)
            : data(data), ptr(ptr)
        {
        }
                
        inline T next()
        {
            T result = 0;
            uint8_t shift = 0;
            while (true) {
                B byte = read_byte();
                if (byte < MSB) {
                    return result | (((T)byte) << shift);
                }
                byte &= NOT_MSB;
                result |= ((T)byte) << shift;
                shift += B_SIGNIFICANT_BITS;
            }
        }

        inline S * get_ptr_internal()
        {
            return &ptr;
        }
    };

    inline Iterator iterator(S start = 0)
    {
        Iterator it(data, start);
        return it;
    }

    class ByteWriter
    {
    protected:
        std::vector<B> * data;
        S ptr;
    public:
        inline ByteWriter(std::vector<B> * data, S ptr) :
            data(data), ptr(ptr)
        {
        }

        inline void write_byte(B byte)
        {
            assert(ptr < data->size());
            (*data)[ptr++] = byte;
        }

        inline void flush() {}

        inline S get_ptr()
        {
            return this->ptr;
        }
    };

    class PushingByteWriter : public ByteWriter
    {
    public:
        PushingByteWriter(std::vector<B> * data, S ptr = 0) :
            ByteWriter(data, ptr)
        {
            assert(ptr == data->size());
        }

        inline void write_byte(B byte)
        {
            ByteWriter::data->push_back(byte);
        }

        inline S get_ptr()
        {
            return (S)ByteWriter::data.size();
        }
    };

    class LockedToIteratorByteWriter : public ByteWriter
    {
    protected:
        static const S PERPETUAL_INFINITY = std::numeric_limits<S>::max();

        const S * iterator_ptr;
        S overheat_buffer_size;
        bool overheat;
        std::vector<B> * backup_data;
        const S * backup_iterator_ptr;
            S & backup_ptr = overheat_buffer_size;
    public:
        inline LockedToIteratorByteWriter(std::vector<B> * data, S ptr, S * iterator_ptr, S overheat_buffer_size) :
            ByteWriter(data, ptr), iterator_ptr(iterator_ptr), overheat_buffer_size(overheat_buffer_size), overheat(false)
        {}

        inline LockedToIteratorByteWriter(std::vector<B> * data, S ptr) :
            ByteWriter(data, ptr), iterator_ptr(&PERPETUAL_INFINITY), overheat_buffer_size(0), overheat(false)
        {}

        inline ~LockedToIteratorByteWriter()
        {
            assert(!overheat);
        }

        inline void write_byte(B byte)
        {
            if (ByteWriter::ptr >= *iterator_ptr) {
// Overheat. Switching to overheating buffer.
                // This happens when we are writing more bytes than was read by the iterator.
                overheat = true;
                backup_data = ByteWriter::data;
                ByteWriter::data = new std::vector<B>(overheat_buffer_size);
                backup_ptr = ByteWriter::ptr; 
                ByteWriter::ptr = 0;
                backup_iterator_ptr = iterator_ptr;
                iterator_ptr = &PERPETUAL_INFINITY;
            }
            ByteWriter::write_byte(byte);
        }

        inline void flush()
        {
            if (!overheat) {
                return;
            }
            assert(backup_ptr + ByteWriter::ptr <= *backup_iterator_ptr );
            // Copying from the overheat buffer back to the main buffer.
            // They were swapped during the overheat, so now
            // data points to the overheat buffer
            // backup_datapoints to the main buffer
            // It is confusing, but it is so for performance reasons.
            std::copy(ByteWriter::data->begin(), ByteWriter::data->begin() + ByteWriter::ptr, backup_data->begin() + backup_ptr);
            delete ByteWriter::data;
            ByteWriter::data = backup_data;
            ByteWriter::ptr = backup_ptr + ByteWriter::ptr;
            iterator_ptr = backup_iterator_ptr;
            overheat = false;
        }

        S get_ptr()
        {
            assert(!this->overheat); // Call flush() to recover after overheat.
            return ByteWriter::get_ptr();
        }
    };

    template<class W>
    class AbstractWriter
    {
    protected:
        W byte_writer;
    public:
        inline AbstractWriter(W byte_writer) :
            byte_writer(byte_writer)
        {}

        inline void write(T value)
        {
            while (value >= MSB) {
                byte_writer.write_byte(MSB | (((B)value) & NOT_MSB));
                value >>= B_SIGNIFICANT_BITS;
            }
            byte_writer.write_byte((B)value);
        }

        inline void flush()
        {
            byte_writer.flush();
        }

        inline S get_ptr()
        {
            return byte_writer.get_ptr();
        }
    };

    /*
    class Writer
    {
    private:
        std::vector<B> * data;
        std::vector<B> * growing_data;
        S ptr;
        S * iterator_ptr;
    protected:
            inline void write_byte(B byte)
        {
            if (growing_data == nullptr) {
                if ((iterator_ptr == nullptr) || (*iterator_ptr > ptr)) {
                    (*data)[ptr++] = byte;
                    return;
                }

                // Hard case that happens rarely.
                // We are writing more bytes than we read, so we have to create a temporary buffer to write to.
                // In the end we will move the data back to where it belongs.
                growing_data = new std::vector<B>();
            }
            growing_data->push_back(byte);
        }

    public:
        inline Writer(std::vector<B> * data, S ptr, S * iterator_ptr) :
            ptr(ptr), iterator_ptr(iterator_ptr), data(nullptr), growing_data(nullptr)
            {
                if (data->size() == ptr) {
                    this->growing_data = data;
                }
                else {
                    this->data = data;
                }
            }

        inline ~Writer()
        {
        }

        inline public void flush()
        {
            if ((growing_data != nullptr) && (data != nullptr)) {
                std::copy(growing_data->begin(), growing_data->end(), data->begin() + ptr);
                ptr += growing_data->size();
                if (iterator_ptr != nullptr) {
                assert(ptr < *iterator_ptr);
            }
                delete growing_data;
                growing_data = nullptr;
            }
        }

        inline void write(T value)
        {
            while (value >= MSB) {
                write_byte(MSB | (((B)value) & NOT_MSB));
                value >>= B_SIGNIFICANT_BITS;
            }
            write_byte((B)value);
        }
    };
    */
    typedef AbstractWriter<PushingByteWriter> InitialWriter;
    typedef AbstractWriter<LockedToIteratorByteWriter> Writer;

    InitialWriter get_initial_writer()
    {
        assert(data.size() == 0);
        PushingByteWriter pbr(&data);
        InitialWriter wr(pbr);
        return wr;
    }

    std::pair<Iterator, Writer> iterator_and_writer(S offset, S overheat_buffer_size = 0)
    {
        if (overheat_buffer_size == 0) {
            overheat_buffer_size = data.size();
        }
        Iterator it = iterator(offset);
        LockedToIteratorByteWriter lbr(&data, offset, it.get_ptr_internal(), overheat_buffer_size);
        Writer wr(lbr);
        return std::pair<Iterator, Writer>(it, wr);
    }

    Writer writer(S offset)
    {
        LockedToIteratorByteWriter lbr(&data, offset);
        Writer wr(lbr);
        return wr;
    }

};

template <typename T, typename B, typename S>
const S VarIntBuffer<T, B, S>::LockedToIteratorByteWriter::PERPETUAL_INFINITY;
#endif /* defined(__tealtree__VAR_INT_BUFFER__) */

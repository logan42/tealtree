#ifndef __tealtree__histogram__
#define __tealtree__histogram__

#include <algorithm>
#include <stdio.h>
#include <vector>

#include "document.h"
#include "types.h"

struct HistogramItem {
    float_t gradient;
    union {
        float_t hessian;
        DOC_ID count;
    };

    inline HistogramItem() : gradient(0)
    {
        if (sizeof(count) >= sizeof(hessian)) {
            count = 0;
        }
        else {
            hessian = 0;
        }
    };
};


template<const bool NEWTON_STEP>
class HistGetter {};

template<>
class HistGetter<false>
{
public:
    typedef DOC_ID WEIGHT_T;
    static inline     WEIGHT_T & get_weight(HistogramItem & item)
    {
        return item.count;
    }

    static inline WEIGHT_T get_unit()
    {
        return 1;
    }

    static inline WEIGHT_T get_regularization_coefficient()
    {
        return 0;
    }

    static WEIGHT_T get_document_weight(Document & doc)
    {
        return 1;
    }
};

template<>
class HistGetter<true>
{
public:
    typedef decltype(HistogramItem::hessian) WEIGHT_T;
    static inline WEIGHT_T & get_weight(HistogramItem & item)
    {
        return item.hessian;
    }

    static inline WEIGHT_T get_unit()
    {
        return EPSILON;
    }

    static inline WEIGHT_T get_regularization_coefficient()
    {
        return 1;
    }

    static WEIGHT_T get_document_weight(Document & doc)
    {
        return doc.hessian;
    }
};



struct Histogram {
    std::vector<HistogramItem> data;
    
    inline Histogram(uint32_t buckets = 0) :
        data(buckets)
    {};
    inline ~Histogram() {};

    inline void subtract(Histogram & other, bool newton_step)
    {
        assert(this->data.size() == other.data.size());
        if (newton_step) {
        for (size_t i = 0; i < this->data.size(); i++) {
            this->data[i].gradient -= other.data[i].gradient;
            this->data[i].hessian-= other.data[i].hessian;
            this->data[i].hessian = std::max<float_t>(this->data[i].hessian, 0);
        }
        }
        else {
            for (size_t i = 0; i < this->data.size(); i++) {
                this->data[i].gradient -= other.data[i].gradient;
                assert(this->data[i].count >= other.data[i].count);
                this->data[i].count -= other.data[i].count;
            }
        }
    }

};

#endif /* defined(__tealtree__histogram__) */

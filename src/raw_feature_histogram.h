#ifndef __tealtree__raw_feature_histogram__
#define __tealtree__raw_feature_histogram__

#include "buckets_collection.h"
#include "raw_feature.h"
#include "types.h"

#include <map>
#include <stdio.h>


#define MY_VALUES_RANGE_HEAP_TYPE boost::heap::d_ary_heap<const ValuesRange<T> *, boost::heap::mutable_<true>, boost::heap::arity<2>, boost::heap::compare<CompareValuesRanges<T>>>

template <typename T>
struct ValuesRange;

template <typename T>
struct CompareValuesRanges {
    inline bool operator()
    (const ValuesRange<T> * r1 , const ValuesRange<T> * r2) const {
        return r1->absorbable_range > r2->absorbable_range;
    }
};

class RawFeatureHistogram
{
protected:
public:
    virtual ~RawFeatureHistogram() = 0;
    virtual uint32_t get_number_of_buckets() const = 0;
    virtual std::vector<UNIVERSAL_BUCKET> * get_bucketized_data() const = 0;
    virtual std::unique_ptr<BucketsCollection> get_buckets() const = 0;
    virtual float_t get_sparsity() const = 0;
    virtual UNIVERSAL_BUCKET get_default_bucket() const = 0;
};

template <typename T>
class RawFeatureHistogramImpl : public RawFeatureHistogram
{
private:
    std::map<T, DOC_ID> hist;
    std::vector<T> bucket_min, bucket_max;
    std::unique_ptr<std::vector<UNIVERSAL_BUCKET>> universal_data;
    float_t sparsity;
    UNIVERSAL_BUCKET default_bucket;
public:
    RawFeatureHistogramImpl();
    virtual ~RawFeatureHistogramImpl();
    void compute_and_bucketize(const RawFeature<T> * raw_feature, uint32_t max_buckets);
    virtual uint32_t get_number_of_buckets() const;
    virtual std::vector<UNIVERSAL_BUCKET> * get_bucketized_data() const;
    virtual std::unique_ptr<BucketsCollection> get_buckets() const;
    virtual float_t get_sparsity() const;
    virtual UNIVERSAL_BUCKET get_default_bucket() const;
private:
    void compute_histogram(const RawFeature<T> * feature);
    void compute_buckets(uint32_t max_buckets);
    void compute_buckets_fast(uint32_t max_buckets);
    void compute_buckets_hard(uint32_t max_buckets);
    void bucketize(const RawFeature<T> * feature);
    void compute_sparsity();
};

template <typename T>
struct ValuesRange
{
    T min_value, max_value;
    T absorbable_range;
    ValuesRange<T> * next_range, * prev_range;
    bool absorbed;
    bool need_update;
    size_t frequency;
    
    ValuesRange(T value, DOC_ID frequency)
        :min_value(value),
        max_value(value),
        absorbable_range(0),
next_range(nullptr),
prev_range(nullptr),
absorbed(false),
need_update(false),
frequency(frequency)
    {}

    //void set_value(T value);
    inline void absorb(ValuesRange<T> * other)
    {
        this->max_value = other->max_value;
        this->next_range = other->next_range;
        if (this->next_range != NULL) {
            this->next_range->prev_range = this;
        }
        this->frequency += other->frequency;
        other->absorbed = true;
    }

    inline void update()
    {
        if (this->next_range != NULL) {
            this->absorbable_range = this->next_range->max_value - this->min_value;
        }
        else {
            // I'm not sure we need this
            this->absorbable_range = this->max_value - this->min_value;
        }
    }
};

#endif /* defined(__tealtree__raw_feature_histogram__) */

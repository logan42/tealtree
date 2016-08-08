#ifndef __tealtree__buckets_collection__
#define __tealtree__buckets_collection__

#include <stdio.h>
#include <vector>

#include "feature_metadata.h"
#include "types.h"

class BucketsCollection
{
private:
public:
    virtual ~BucketsCollection();
    virtual std::string get_bucket_as_string(uint32_t bucket_id) = 0;
    virtual FeatureValue get_bucket_value(uint32_t bucket_id) = 0;
    virtual RawFeatureType get_type() = 0;
    virtual FeatureMetadata create_metadata() = 0;
};

template<typename T>
class BucketsCollectionImpl : public BucketsCollection
{
private:
    std::vector<T> bucket_min;
public:
    BucketsCollectionImpl(const std::vector<T> & bucket_min);
    virtual ~BucketsCollectionImpl();
    virtual std::string get_bucket_as_string(uint32_t bucket_id);
    virtual FeatureValue get_bucket_value(uint32_t bucket_id);
    virtual RawFeatureType get_type();
    void set_buckets(const std::vector<T> & bucket_min);
    virtual FeatureMetadata create_metadata();
private:
    T adjust_value(T value);
};

class BucketsProvider
{
    public:
        virtual ~BucketsProvider();
            virtual BucketsCollection * get_buckets(uint32_t feature_id) = 0;
};

#endif /* defined(__tealtree__buckets_collection__) */

#include "buckets_collection.h"
#include "util.h"

BucketsCollection::~BucketsCollection()
{
}


template<typename T>
BucketsCollectionImpl<T>::BucketsCollectionImpl(const std::vector<T> & bucket_min)
{
    this->set_buckets(bucket_min);
}

template<typename T>
BucketsCollectionImpl<T>::~BucketsCollectionImpl    ()
{
}


template<typename T>
std::string BucketsCollectionImpl<T>::get_bucket_as_string(uint32_t bucket_id)
{
    return to_string(this->bucket_min[bucket_id]);
}

template<typename T>
T BucketsCollectionImpl<T>::adjust_value(T value)
{
    return value;
}

template<>
float_t BucketsCollectionImpl<float_t>::adjust_value(float_t value)
{
    return std::nextafter(value, - std::numeric_limits<float_t>::infinity());
}

template<>
double_t BucketsCollectionImpl<double_t>::adjust_value(double_t value)
{
    // This is not needed, but throw an exception just in case to alert whomever 
    // if this is ever going to be used.
    throw std::runtime_error("Not implemented");
}

template<typename T>
FeatureValue BucketsCollectionImpl<T>::get_bucket_value(uint32_t bucket_id)
{
    FeatureValue result;
    T * t = reinterpret_cast<T*>(&result);
    *t = this->adjust_value(this->bucket_min[bucket_id]);
    return result;
}

template<typename T>
RawFeatureType BucketsCollectionImpl<T>::get_type()
{
    return get_feature_type_from_template<T>();
}

template<typename T>
void BucketsCollectionImpl<T>::set_buckets(const std::vector<T> & bucket_min)
{
    this->bucket_min = bucket_min;
}

template<typename T>
FeatureMetadata BucketsCollectionImpl<T>::create_metadata()
{
    FeatureMetadata result;
    result.set_impl(std::unique_ptr<AbstractFeatureMetadataImpl>(new FeatureMetadataImpl<T>()));
    return result;
}

INSTANTITATE_TEMPLATES_FOR_RAW_FEATURE_TYPES(BucketsCollectionImpl)

BucketsProvider::~BucketsProvider()
{
}


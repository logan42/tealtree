#include "feature.h"
#include "split.h"

std::vector<FeatureMetadata> const * SplitLite::current_metadata;

SplitLite::SplitLite() : 
    feature(0), 
    threshold(), 
    inverse(false) { };
SplitLite::SplitLite(Split & split, BucketsProvider * buckets_provider) : 
    feature(split.feature->get_index()), 
    inverse(split.inverse) 
{
    threshold = buckets_provider->get_buckets(split.feature->get_index())->get_bucket_value(split.threshold);
};

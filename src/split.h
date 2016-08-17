#ifndef __tealtree__split__
#define __tealtree__split__

#include <stdio.h>
#include <vector>

#include "buckets_collection.h"
#include "compact_vector.h"
#include "types.h"

class Feature;

struct TreeNode;

struct Split {
    float_t spread;
    Feature * feature;
    TreeNode * node;
    UNIVERSAL_BUCKET threshold;
    // If the split is NOT inverse then documents >= threshold go to the right node.
    // If the split is inverse then documents < threshold go to the right node.
    bool inverse;
    
    Split()
        : spread(-1),
        feature(NULL),
        node(NULL),
        threshold(0),
        inverse(false)
    { };
    Split(float_t spread, UNIVERSAL_BUCKET threshold, TreeNode * node, Feature* feature, bool inverse = false)
        : spread(spread),
        feature(feature),
        node(node),
        threshold(threshold),
        inverse(inverse)
    { };
    Split(const Split & other) = default;
};

struct SplitLite 
{
    // This is an ugly hack. We need to know the feature metadata information to serialize the feature value.
    static std::vector<FeatureMetadata> const * current_metadata;

    FEATURE_INDEX feature;
    FeatureValue threshold;
    bool inverse;
    
    SplitLite();
    SplitLite(Split & split, BucketsProvider * buckets_provider);

    template<class Archive>
    void save(Archive & archive) const
    {
        std::string threshold = (*current_metadata)[this->feature].value_to_string(this->threshold);
        archive(
            CEREAL_NVP(feature), 
            CEREAL_NVP(threshold),
            CEREAL_NVP(inverse));
    }

    template<class Archive>
    void load(Archive & archive) 
    {
        std::string threshold;
        archive(
            CEREAL_NVP(feature),
            CEREAL_NVP(threshold),
            CEREAL_NVP(inverse));
        this->threshold = (*current_metadata)[this->feature].string_to_value(threshold);
    }

};

// Split signature is essentially a bit mask, where each bit denotes whether
// the document is going to the left or right leaf. We use vector for convenience.
typedef CompactVector<1> SplitSignature;

#endif /* defined(__tealtree__split__) */

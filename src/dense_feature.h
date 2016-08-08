#ifndef __tealtree__dense_feature__
#define __tealtree__dense_feature__

#include "compact_vector.h"
#include "feature.h"
#include "types.h"

#include <stdio.h>
#include <vector>

class DenseFeature : public Feature
{
};

template<const uint8_t BITS>
class DenseFeatureImpl : public DenseFeature
{
protected:
    typedef CompactVector<BITS> CV;
    typedef typename CV::ValueType ValueType;
    CV cv;
    virtual UNIVERSAL_BUCKET get_value(DOC_ID doc_id);
public:
    virtual ~DenseFeatureImpl();
    virtual const std::string get_registry_name();
    void virtual init_from_raw_histogram(const RawFeatureHistogram * hist);
    virtual std::unique_ptr<SplitSignature> get_split_signature(TreeNode * leaf, Split * split);
    virtual std::unique_ptr<Histogram> compute_histogram(const TreeNode * leaf, bool newton_step);
    template <const bool NEWTON_STEP>
    std::unique_ptr<Histogram> compute_histogram_impl(const TreeNode * leaf);

};

//std::unique_ptr<DenseFeature> create_dense_feature_from_histogram(const RawFeatureHistogram * hist);

#endif /* defined(__tealtree__dense_feature__) */

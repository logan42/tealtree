#ifndef __tealtree__SPARSE_feature__
#define __tealtree__SPARSE_feature__

#include "compact_vector.h"
#include "feature.h"
#include "histogram.h"
#include "types.h"
#include "var_int_buffer.h"

#include <stdio.h>
#include <vector>

class SparseFeature : public Feature
{
};

template<const uint8_t BITS>
class SparseFeatureImpl : public SparseFeature
{
protected:
    typedef CompactVector<BITS> CV;
    typedef typename CV::ValueType ValueType;
    CV cv;
    ValueType default_value;
    typedef VarIntBuffer<DOC_ID, uint8_t, DOC_ID> VIB;
    VIB offsets;
    virtual UNIVERSAL_BUCKET get_value(DOC_ID doc_id);
protected:
    template<typename U>
    void compute_on_values(const TreeNode * leaf, U & updater, DOC_ID n_docs,  DOC_ID v_ptr = 0, DOC_ID o_ptr = 0);
    template<const bool NEWTON_STEP>
    inline std::unique_ptr<Histogram> compute_histogram_impl(const TreeNode * leaf);
    template<const bool NEWTON_STEP>
    inline void fix_histogram(const TreeNode * leaf, Histogram * hist)
    {
        typedef HistGetter<NEWTON_STEP> HG;
        HistogramItem & default_item = hist->data[this->default_value];
        if (!NEWTON_STEP) {
            default_item.count = leaf->doc_ids.size();
        }
        else {
            default_item.hessian = leaf->sum_hessian;
        }
        default_item.gradient = leaf->sum_gradient;
        for (size_t i = 0; i < this->n_buckets; i++) {
            if (i != this->default_value) {
                HG::get_weight(default_item) -= HG::get_weight(hist->data[i]);
                default_item.gradient -= hist->data[i].gradient;
            }
        }
    }

public:
    virtual ~SparseFeatureImpl() {};
    virtual const std::string get_registry_name();
    void virtual init_from_raw_histogram(const RawFeatureHistogram * hist);
    virtual std::unique_ptr<SplitSignature> get_split_signature(TreeNode * leaf, Split * split);
    virtual std::unique_ptr<Histogram> compute_histogram(const TreeNode * leaf, bool newton_step);
};

#endif /* defined(__tealtree__SPARSE_feature__) */

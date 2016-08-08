#ifndef jadetree_feature_h
#define jadetree_feature_h

#include <stdlib.h>

#include "histogram.h"
#include "tree_node.h"
#include "raw_feature_histogram.h"
#include "registry.h"
#include "split.h"

struct TrainerData;

class Feature   
{
public:
    static Registry<Feature> registry;
    static std::unique_ptr<Feature> create(const std::string & class_name)
    {
        return registry.create(class_name);
    }
    virtual const std::string get_registry_name() = 0;

protected:
    std::string name;
    TrainerData * trainer_data;
    FEATURE_INDEX index;
    std::unique_ptr<BucketsCollection> buckets;
    uint32_t n_buckets = 0;
    virtual UNIVERSAL_BUCKET get_value(DOC_ID doc_id) = 0;
public:
    const std::string & get_name() const;
    void set_name(std::string name);
    FEATURE_INDEX get_index() const;
    void set_index(FEATURE_INDEX index);
    void set_trainer_data(TrainerData * trainer_data);
    void virtual init_from_raw_histogram(const RawFeatureHistogram * hist) = 0;
    virtual std::unique_ptr<Histogram> compute_histogram(const TreeNode * leaf, bool newton_step) = 0;
    virtual std::unique_ptr<SplitSignature> get_split_signature(TreeNode * leaf, Split * split) = 0;
    virtual void on_finalize_tree() {}
    virtual ~Feature();
    BucketsCollection * get_buckets();
    void set_buckets(std::unique_ptr<BucketsCollection> buckets);
    FeatureMetadata create_metadata();
};

std::unique_ptr<Feature> create_feature_from_histogram(const RawFeatureHistogram * hist, float_t sparsity_threshold);

#endif

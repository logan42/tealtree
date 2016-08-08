#ifndef __tealtree__tree__
#define __tealtree__tree__

#include <stdio.h>

#include "buckets_collection.h"
#include "cereal.h"
#include "feature_metadata.h"
#include "tree_node.h"

struct TrainerData;

class Tree
{
private:
    std::vector<std::unique_ptr<TreeNode>> nodes;
    bool debug_info;
public:
    Tree(TrainerData * data, bool debug_info);
    TreeNode * get_root();
    std::vector<std::unique_ptr<TreeNode>> & get_nodes();
    std::pair<TreeNode*, TreeNode*> split_node(TreeNode * leaf, SplitSignature * split_signature);
};

class TreeLite
{
private:
    std::vector<TreeNodeLite> nodes;
public:
    TreeLite() {}
    TreeLite(Tree & tree, BucketsProvider * buckets_provider);
    //TreeLite(const TreeLite & other);
    TreeLite(TreeLite && other);
    const std::vector<TreeNodeLite> & get_nodes() const;

    template<class Archive>
    void serialize(Archive & archive)
    {
        archive(
            CEREAL_NVP(nodes));
    }

};

class Ensemble
{
private:
    std::string cost_function;
    std::vector<FeatureMetadata> features;
    std::vector<TreeLite> trees;
public:
    Ensemble()
    {}

    void add_feature(FeatureMetadata feature)
    {
        this->features.push_back(std::move(feature));
    }

    void add_tree(TreeLite & tree)
    {
        this->trees.push_back(std::move(tree));
    }

    const std::vector<FeatureMetadata> & get_features() const
    {
        return this->features;
    }
    const std::vector<TreeLite> & get_trees() const
    {
        return this->trees;
    }

    void set_cost_function(const std::string & cf)
    {
        this->cost_function = cf;
    }

    const std::string & get_cost_function() const
    {
        return this->cost_function;
    }

    template<class Archive>
    void serialize(Archive & archive)
    {
        archive(
            CEREAL_NVP(cost_function),
            CEREAL_NVP(features));
        assert(SplitLite::current_metadata == nullptr);
        SplitLite::current_metadata = &this->features;
        archive(
            CEREAL_NVP(trees));
        assert(SplitLite::current_metadata == &this->features);
        SplitLite::current_metadata = nullptr;
    }

};

#endif /* defined(__tealtree__tree__) */

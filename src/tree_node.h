#ifndef __tealtree__leaf__
#define __tealtree__leaf__

#include <memory>
#include <stdio.h>
#include <unordered_map>
#include <vector>

#include "cereal.h"
#include "histogram.h"
#include "split.h"
#include "types.h"

struct TreeNodeDebugInfo
{
    DOC_ID n_docs;
    float_t spread;
    std::string split_feature;
    std::string split_threshold;

    template<class Archive>
    void save(Archive & archive) const 
    {
        archive(
            CEREAL_NVP(n_docs),
            CEREAL_NVP(spread));
        if (split_feature.size() > 0) {
        archive(
            CEREAL_NVP(split_feature));
    }
    if (split_threshold.size() > 0) {
        archive(
            CEREAL_NVP(split_threshold));
    }
    }

    template<class Archive>
    void load(Archive & archive)
    {
// Do nothing. This function is not even supposed to be called.
    }

};;

struct TreeNode;

struct TreeNode {
    TREE_NODE_ID node_id;
    TreeNode * parent;
    TreeNode * left, * right;
    std::vector<DOC_ID> doc_ids;
    std::unique_ptr<Split> split;
    float_t leaf_value;
    std::unique_ptr<std::vector<std::unique_ptr<Histogram>>> histograms;
    std::unique_ptr<TreeNodeDebugInfo> debug_info;
    float_t sum_gradient;
    float_t sum_hessian;

    std::unique_ptr<SplitSignature> split_signature;
    std::unique_ptr<std::vector<DOC_ID>> split_mapping;
    
    TreeNode() :
        node_id(0),
        parent(NULL),
        left(NULL),
        right(NULL),
        split(nullptr),
        leaf_value(0),
        histograms(nullptr),
        debug_info(nullptr),
        sum_gradient(0),
        sum_hessian(0),
        split_signature(nullptr),
        split_mapping(nullptr)
        {};
    
    bool is_leaf()
    {
        if (this->left == NULL) {
            assert(this->right == NULL);
            return true;
        }
        assert(this->right != NULL);
        return false;
    }

    uint32_t get_depth()
    {
        uint32_t result = 0;
        TreeNode * node = this;
        while (node->parent != nullptr) {
            result++;
            node = node->parent;
        }
        return result;
    }
};

struct TreeNodeLite 
{
    TREE_NODE_ID left_id, right_id;    
        SplitLite split;
        float_t value;
    std::unique_ptr<TreeNodeDebugInfo> debug_info;

    TreeNodeLite() {};
    TreeNodeLite(const TreeNodeLite & other) :
        left_id(other.left_id),
        right_id(other.right_id),
        split(other.split),
        value(other.value)
    {
        if (other.debug_info != nullptr) {
            debug_info = std::unique_ptr<TreeNodeDebugInfo>(new TreeNodeDebugInfo(*(other.debug_info)));
        }
    }
    TreeNodeLite(TreeNodeLite && other) :
        left_id(other.left_id),
        right_id(other.right_id),
        split(other.split),
        value(other.value),
        debug_info(std::move(other.debug_info))
    {}

    TreeNodeLite(TreeNode & node, BucketsProvider * buckets_provider)
    {
        this->left_id  = (node.left  != NULL) ? node.left ->node_id : 0;
        this->right_id = (node.right != NULL) ? node.right->node_id : 0;
        if (node.is_leaf()) {
            this->value = node.leaf_value;
        } else {
            this->split = SplitLite(*node.split, buckets_provider);
                this->value = std::numeric_limits<float_t>::quiet_NaN();
        }
        this->debug_info = std::move(node.debug_info);
    }

    TreeNodeLite& operator=(TreeNodeLite && other)
    {
        left_id = other.left_id;
        right_id = other.right_id;
        split = other.split;
        value = other.value;
        debug_info = std::move(other.debug_info);
        return *this;
    }

    TreeNodeLite& operator=(const TreeNodeLite & other)
    {
        left_id = other.left_id;
        right_id = other.right_id;
        split = other.split;
        value = other.value;
        if (other.debug_info != nullptr) {
        debug_info = std::unique_ptr<TreeNodeDebugInfo>(new TreeNodeDebugInfo(*(other.debug_info)));
    }
        return *this;
    }

    template<class Archive>
    void save (Archive & archive) const
    {
        if (std::isfinite(value)) {
            archive(
                CEREAL_NVP(value));
        }
        else {
            archive(
                CEREAL_NVP(left_id),
                CEREAL_NVP(right_id),
                CEREAL_NVP(split));
        }
        if (debug_info != nullptr) {
            archive(
                cereal::make_nvp("debug_info", *debug_info));
        }
        }

    template<class Archive>
    void load(Archive & archive)
    {
        try {
            archive(
                CEREAL_NVP(value));
        }
        catch(cereal::Exception &e){
            (void)e;
            // It must be a non-leaf node. Read the split instead.
            value = std::numeric_limits<float_t>::quiet_NaN();;
            archive(
                CEREAL_NVP(left_id),
                CEREAL_NVP(right_id),
                CEREAL_NVP(split));
        }
    }

};

#endif /* defined(__tealtree__leaf__) */

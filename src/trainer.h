#ifndef __tealtree__trainer__
#define __tealtree__trainer__

#include <stdio.h>
#include <mutex>

#include "cost_function.h"
#include "document.h"
#include "feature.h"
#include "tree_node.h"
#include "raw_feature.h"
#include "split.h"
#include "thread_pool.h"
#include "trainer_data.h"

struct TrainerParams
{
    bool newton_step;
    bool quadratic_spread;
    float_t regularization_lambda;
    DOC_ID min_node_docs;
    float_t min_node_hessian;
    bool tree_debug_info;
};


class Trainer
{
private:
    TrainerData data;
    std::vector<std::unique_ptr<Feature>> features;
    std::mutex mutex;
    ThreadPool * tp;
    std::unique_ptr<CostFunction> cost_function;
    TrainerParams params;
public:
    Trainer();
    TrainerData * get_data();
    void load_documents(const std::vector<float_t> * labels, const std::vector<DOC_ID> * query_limits);
    void add_feature(std::unique_ptr<Feature> feature);
    Feature * get_feature(size_t feature_id);
    size_t get_features_count();
    void set_cost_function(std::unique_ptr<CostFunction> cost_function);
    void set_thread_pool(ThreadPool * tp);
    void set_parameters(const TrainerParams & params);
    void start_ensemble();
    void start_new_tree();
    void compute_histograms(TreeNode * node, TreeNode * sibling, std::unique_ptr<SplitSignature> last_split_signature);
    std::unique_ptr<SplitSignature> get_split_signature(Split * split);
    std::pair<TreeNode*, TreeNode*> split_node(TreeNode * leaf, SplitSignature * split_signature, bool will_compute_children_histograms);
    void set_base_score(float_t base_score);
    void finalize_tree(float_t step_alpha);
    void clear_tree();
private:
    std::pair<Split, Split> compute_histogram_feature(TreeNode * node, TreeNode * sibling, Feature * feature);
    inline std::pair<float_t, uint32_t> find_best_split_feature(Histogram * hist, TreeNode * node, Feature * feature);
    template<const bool NEWTON_STEP>
    std::pair<float_t, uint32_t> find_best_split_feature_impl(Histogram * hist, TreeNode * node, Feature * feature);
    void finalize_node(float_t step_alpha, TreeNode * node);
};

#endif /* defined(__tealtree__trainer__) */

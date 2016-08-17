#include "fast_sparse_feature.h"
#include "log_trivial.h"
#include "trainer.h"

#ifdef _WIN32
#  include <intrin.h>
#  define __builtin_popcount __popcnt
#endif


Trainer::Trainer()
{
    
}

TrainerData * Trainer::get_data()
{
    return &this->data;
}

void Trainer::load_documents(const std::vector<float_t> * labels, const std::vector<DOC_ID> * query_limits)
{
    const std::vector<float_t> & data = *labels;
    size_t size = data.size();
    this->data.documents.resize(size);
    for (size_t i = 0; i < size; i++) {
        this->data.documents[i].doc_id = (DOC_ID)i;
        this->data.documents[i].target_score = data[i];
    }
    this->data.query_limits = *query_limits;
}

void Trainer::add_feature(std::unique_ptr<Feature> feature)
{
    feature->set_trainer_data(&(this->data));
    feature->set_index((FEATURE_INDEX)this->features.size());
    this->features.push_back(std::move(feature));
}

Feature * Trainer::get_feature(size_t feature_id)
{
    return this->features[feature_id].get();
}

size_t Trainer::get_features_count()
{
    return this->features.size();
}


void Trainer::set_cost_function(std::unique_ptr<CostFunction> cost_function)
{
    this->cost_function = std::move(cost_function);
}

void Trainer::set_thread_pool(ThreadPool * tp)
{
    this->tp = tp;
}

void Trainer::set_parameters(const TrainerParams & params)
{
    this->params = params;
}

void Trainer::start_ensemble()
{
    this->cost_function->precompute(&this->data);
}

void Trainer::start_new_tree()
{
    assert(this->data.current_tree.get() == NULL);
    this->data.current_tree = std::unique_ptr<Tree>(new Tree(&this->data, this->params.tree_debug_info));
    FastShardMapping::get_instance().on_start_new_tree(this->data.current_tree->get_root());
    this->cost_function->compute_gradient(&this->data, this->params.newton_step, this->tp);
    for (size_t i = 0; i < this->data.documents.size(); i++) {
        assert(std::isfinite(this->data.documents[i].gradient));
        if (this->params.newton_step) {
            assert(std::isfinite(this->data.documents[i].hessian));
            assert(this->data.documents[i].hessian >= 0);
        }
    }
}


std::pair<Split, Split> Trainer::compute_histogram_feature(TreeNode * node, TreeNode * sibling, Feature * feature)
{
    std::unique_ptr<Histogram> hist = feature->compute_histogram(node, this->params.newton_step);
    Histogram * sibling_hist = nullptr;
    std::pair<float_t, uint32_t> best_split = this->find_best_split_feature(hist.get(), node, feature);
    std::pair<float_t, uint32_t> best_split_sibling;
    
    if (sibling != NULL) {
        // Reading a vector element that is not being written by any other thread..
        // We can do that without any locks.
         sibling_hist = (*sibling->histograms)[feature->get_index()].get();
        sibling_hist->subtract(*hist, this->params.newton_step);
        best_split_sibling = this->find_best_split_feature(sibling_hist, sibling, feature);
    }
    
    std::pair<Split, Split> result;
    result.first = Split(best_split.first, best_split.second, node, feature, false);
    if (sibling != nullptr) {
        result.second= Split(best_split_sibling.first, best_split_sibling.second, sibling, feature, false);
    }
    {
        std::lock_guard<std::mutex> guard(this->mutex);
        (*node->histograms)[feature->get_index()] = std::move(hist);
    }
    return result;
}

inline std::pair<float_t, uint32_t> Trainer::find_best_split_feature(Histogram * hist, TreeNode * node, Feature * feature)
{
    if (this->params.newton_step) {
        return this->find_best_split_feature_impl<true>(hist, node, feature);
    }
    else {
        return this->find_best_split_feature_impl<false>(hist, node, feature);
    }
 }

template<const bool NEWTON_STEP>
std::pair<float_t, uint32_t> Trainer::find_best_split_feature_impl(Histogram * hist, TreeNode * node, Feature * feature)
{
    assert(NEWTON_STEP == this->params.newton_step);
    typedef HistGetter<NEWTON_STEP> HG;
    typedef typename HG::WEIGHT_T WEIGHT_T;
    const float_t regularization_lambda = this->params.quadratic_spread ? this->params.regularization_lambda : 0;
    WEIGHT_T min_node_weight;
    float_t total_grad = node->sum_gradient;
    WEIGHT_T total_weight;
    if (NEWTON_STEP) {
        min_node_weight = (WEIGHT_T) this->params.min_node_hessian;
        total_weight = (WEIGHT_T) node->sum_hessian;
    }
    else {
        min_node_weight = (WEIGHT_T)this->params.min_node_docs;
        total_weight = (WEIGHT_T) node->doc_ids.size();
    }

    size_t m = hist->data.size() - 1;
    assert(hist->data.size() >= 1);
    float_t best_spread = -1;
    uint32_t best_spread_bucket = 0;
    WEIGHT_T left_weight = 0;
    float_t left_grad = 0;
    for (size_t i = 0; i < m; i++) {
        left_weight += HG::get_weight(hist->data[i]);
        WEIGHT_T right_weight = total_weight - left_weight;
        left_grad += hist->data[i].gradient;
        float_t right_grad = total_grad - left_grad;
        if ((left_weight < min_node_weight) || (right_weight < min_node_weight )) {
            continue;
        }
        assert(left_weight >= HG::get_unit());
        assert(right_weight >= HG::get_unit());
        float_t left_weighted_grad = left_grad / (left_weight + regularization_lambda);
        float_t right_weighted_grad = right_grad / (right_weight + regularization_lambda);
        float_t total_weighted_grad = total_grad / total_weight;
        float_t split_spread;
        if (!this->params.quadratic_spread) {
            split_spread = std::abs(left_weighted_grad - right_weighted_grad) ;
        }
        else {
            split_spread =
                left_weighted_grad * left_grad
                + right_weighted_grad * right_grad
                 - total_weighted_grad * total_grad;
        }
        if (split_spread > best_spread) {
            best_spread = split_spread;
            best_spread_bucket = (uint32_t)i + 1;
        }
    }
    
    return std::make_pair(best_spread, best_spread_bucket);
}

void Trainer::compute_histograms(TreeNode * node, TreeNode * sibling, std::unique_ptr<SplitSignature> last_split_signature)
{
    assert(node != NULL);
    node->split = std::unique_ptr<Split>(new Split());
    node->histograms = std::unique_ptr<std::vector<std::unique_ptr<Histogram>>>(new std::vector<std::unique_ptr<Histogram>>(this->features.size()));
    if (sibling != NULL) {
        assert(node->parent == sibling->parent);
        assert(last_split_signature != nullptr);
        sibling->split = std::unique_ptr<Split>(new Split());
        sibling->histograms = std::move(node->parent->histograms);

        // Set split signature and compute the mapping
        TreeNode * parent = node->parent;
        DOC_ID n_docs = last_split_signature->size();
        std::unique_ptr<std::vector<DOC_ID>> mapping(new std::vector<DOC_ID>(n_docs));
        DOC_ID counters[2] = { 0, 0 };
        auto it = last_split_signature->iterator();
        for (DOC_ID i = 0; i < n_docs; i++) {
            (*mapping)[i] = counters[it.next()]++;
        }
        parent->split_signature = std::move(last_split_signature);
        parent->split_mapping = std::move(mapping);
    }
    else {
        assert(last_split_signature == nullptr);
    }

    float_t sum_grad = 0, sum_hess=0;
    for (size_t i = 0; i < node->doc_ids.size(); i++) {
        sum_grad += data.documents[node->doc_ids[i]].gradient;
        sum_hess += data.documents[node->doc_ids[i]].hessian;
    }
    node->sum_gradient = sum_grad;
    if (this->params.newton_step) {
        node->sum_hessian= sum_hess;
    }
    if (sibling != nullptr) {
        sibling->sum_gradient = node->parent->sum_gradient - node->sum_gradient;
        if (this->params.newton_step) {
            sibling->sum_hessian= node->parent->sum_hessian - node->sum_hessian;
        }
    }
    std::vector<std::future<std::pair<Split, Split>>> futures;
    futures.reserve(this->features.size());
    for (size_t i = 0; i < this->features.size(); i++) {
        Feature * feature = this->features[i].get();
        futures.push_back(this->tp->enqueue(false, &Trainer::compute_histogram_feature, this, node, sibling, feature));
    }
    for (size_t i = 0; i < this->features.size(); i++) {
        std::pair<Split, Split> pair = futures[i].get();
        if (pair.first.spread > node->split->spread) {
            *node->split = pair.first;
        }
        if (sibling != nullptr){
            if (pair.second.spread > sibling->split->spread) {
                *sibling->split = pair.second;
            }
        }
    }
    if (sibling != nullptr) {
        TreeNode * parent = node->parent;
        parent->split_signature.reset();
        parent->split_mapping.reset();

        // Clearing the doc_ids as well
        std::vector<DOC_ID> empty_doc_ids(0);
        parent->doc_ids.swap(empty_doc_ids);
    }
}

// This function contains a dirty hack inside. It may implicitly modify
// split, so that the left leaf becomes heavier than the right one.
std::unique_ptr<SplitSignature> Trainer::get_split_signature(Split * split)
{
    //Bit count function: __builtin_popcount
    TreeNode * node = split->node;
    std::unique_ptr<SplitSignature> ss = split->feature->get_split_signature(node, split);
    
    // Make sure that the left leaf is heavier (or same) as the right leaf
    DOC_ID hist[2] = { 0,0 };
    auto it = ss->iterator();
    for (DOC_ID i = 0; i < ss->size(); i++) {
        hist[it.next()]++;
    }
    if (hist[1] > hist[0]) {
        split->inverse = true;
        
        // Rather just get the signature again
        ss.reset();
        ss = split->feature->get_split_signature(node, split);
#ifndef NDEBUG
        DOC_ID hist[2] = { 0,0 };
        auto it = ss->iterator();
        for (DOC_ID i = 0; i < ss->size(); i++) {
            hist[it.next()]++;
        }
        assert(hist[1] <= hist[0]);
#endif
    }
    
    return ss;
}

std::pair<TreeNode*, TreeNode*> Trainer::split_node(TreeNode * node, SplitSignature * split_signature, bool will_compute_children_histograms)
{
    if (node->debug_info != nullptr) {
        node->debug_info->n_docs = (DOC_ID)node->doc_ids.size();
        node->debug_info->spread = node->split->spread;
    }

    auto pair = this->data.current_tree->split_node(node, split_signature);
    if (will_compute_children_histograms) {
    FastShardMapping::get_instance().split_tree_node(node);
}
    return pair;
}

void Trainer::set_base_score(float_t base_score)
{
    std::vector<std::unique_ptr<TreeNode>> & nodes = this->data.current_tree->get_nodes();
    assert(nodes.size() == 1);
        nodes[0]->leaf_value = base_score;
    for (size_t i = 0; i < nodes[0]->doc_ids.size(); i++) {
        this->data.documents[nodes[0]->doc_ids[i]].score += base_score;
    }
    FastShardMapping::get_instance().on_finalize_tree();
}

void Trainer::finalize_tree(float_t step_alpha)
{
    std::vector<std::unique_ptr<TreeNode>> & nodes = this->data.current_tree->get_nodes();
    std::vector<std::future<void>> futures;
    futures.reserve(this->features.size() + nodes.size());
    for (size_t i = 0; i < this->features.size(); i++) {
        Feature * feature = this->features[i].get();
        futures.push_back(this->tp->enqueue(false, &Feature::on_finalize_tree, feature));
    }
    for (size_t i = 0; i < nodes.size(); i++) {
        if (nodes[i]->is_leaf()) {
            futures.push_back(this->tp->enqueue(false, &Trainer::finalize_node, this, step_alpha, nodes[i].get()));
        }
    }
    for (size_t i = 0; i < futures.size(); i++) {
        futures[i].get();
    }
    FastShardMapping::get_instance().on_finalize_tree();
}

void Trainer::clear_tree()
{
this->data.current_tree.reset();
}

void Trainer::finalize_node(float_t step_alpha, TreeNode * node)
{
    if (node->debug_info.get() != nullptr) {
        node->debug_info->n_docs = (DOC_ID)node->doc_ids.size();
        if (node->split != nullptr) {
            node->debug_info->spread = node->split->spread;
            std::string feature_name;
            std::string threshold = "N/A";
            if (node->split->feature != nullptr) {
                feature_name = node->split->feature->get_name();
                if (node->split->feature->get_buckets() != nullptr) {
                    threshold = node->split->feature->get_buckets()->get_bucket_as_string(node->split->threshold);
                }
            }
            else {
                feature_name = "<null>";
            }
            node->debug_info->split_feature = feature_name;
            node->debug_info->split_threshold = threshold;
        }
    }

    const float_t regularization_lambda = this->params.quadratic_spread ? this->params.regularization_lambda : 0;
    float_t avg_grad = 0;
    for (size_t i = 0; i < node->doc_ids.size(); i++) {
        avg_grad += this->data.documents[node->doc_ids[i]].gradient;
    }
    if (this->params.newton_step) {
        float_t avg_hessian= 0;
        for (size_t i = 0; i < node->doc_ids.size(); i++) {
            avg_hessian += this->data.documents[node->doc_ids[i]].hessian;
        }
        avg_hessian += regularization_lambda;
        assert(avg_hessian >= EPSILON);
        avg_grad /= avg_hessian;
    }
    else {
        avg_grad /= node->doc_ids.size();// + regularization_lambda;
    }
    if (!std::isfinite(avg_grad)) {
        throw std::runtime_error("Something went wrong. Leaf value is not a number. This is not supposed to happen.");
    }
    assert(std::isfinite(avg_grad));
    
    // We minimize cost function, so the step is in the opposite direction from the gradient.
    float_t step = - avg_grad * step_alpha;
    node->leaf_value = step;
    for (size_t i = 0; i < node->doc_ids.size(); i++) {
        this->data.documents[node->doc_ids[i]].score += step;
    }
}



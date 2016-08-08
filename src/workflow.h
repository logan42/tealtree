#ifndef __tealtree__workflow__
#define __tealtree__workflow__

#include <atomic>
#include <boost/heap/d_ary_heap.hpp>
#include <future>
#include <random>
#include <stdio.h>

#include "blocking_queue.h"
#include "feature.h"
#include "options.h"
#include "raw_feature.h"
#include "split.h"
#include "thread_pool.h"
#include "tree_io.h"
#include "trainer.h"
#include "tsv_reader.h"


class Workflow;

struct CompareSplits {
    inline bool operator()
    (const Split * s1 , const Split * s2) const {
        return s1->spread < s2->spread;
    }
};




class Workflow
{
private:
    Options & options;
    std::unique_ptr<boost::threadpool::pool> thread_pool;
    std::unique_ptr<std::mt19937 > random_engine;
    std::unique_ptr<Trainer> trainer;
    typedef BlockingBoundedQueue <std::future<std::unique_ptr<Feature>>> FEATURE_PIPELINE_TYPE;
    typedef std::shared_ptr<FEATURE_PIPELINE_TYPE> FEATURE_PIPELINE_PTR_TYPE;
    typedef typename boost::heap::d_ary_heap<const Split *, boost::heap::mutable_<true>, boost::heap::arity<2>, boost::heap::compare<CompareSplits>> heap_t;
    typedef typename heap_t::handle_type handle_t;
    heap_t splits_heap;
    std::unique_ptr<TreeWriter> tree_writer;
    std::unique_ptr<BucketsProvider> buckets_provider;
    std::unique_ptr<Ensemble> ensemble;
    bool msg_tree_too_short = false;
    bool msg_score_too_large = false;
public:
    Workflow(Options & options);
    void run();

    void run_train();
    void run_evaluate();
    boost::threadpool::pool * get_thread_pool();
    uint32_t get_concurrency();
    uint32_t get_bbq_size();
    void log_gradient();
    void train_ensemble();
    void set_base_score();
    void train_a_tree(uint32_t tree_index);
    Trainer * get_trainer();
private:
    void init_registries();
    void init_logging();
    void init();
    void check_for_overflow();
    std::unique_ptr<std::vector<std::string>> get_feature_names();
    std::pair<std::unique_ptr<LineReader>, std::string>get_line_reader();
    std::unique_ptr<DataFileReader> get_tsv_reader(std::shared_ptr<ColumnConsumerProvider>  ccp);
    std::tuple<const std::vector<float_t>, const std::vector<DOC_ID>, FEATURE_PIPELINE_PTR_TYPE  > read_tsv();
    FEATURE_PIPELINE_PTR_TYPE  cook_features(std::vector<std::unique_ptr<DynamicRawFeature>> drfs);
    std::unique_ptr<Feature> cook_feature(std::unique_ptr<DynamicRawFeature> drf);
    std::unique_ptr<std::vector<float_t>> preprocess_labels(std::unique_ptr<std::vector<float_t>> labels);
    void log_feature_types(std::map<std::string, size_t> & feature_types, char prefix, std::string msg_prefix);
    std::unique_ptr<Trainer> create_trainer(const std::vector<float_t> * labels, const std::vector<DOC_ID> * query_limits, FEATURE_PIPELINE_PTR_TYPE  features);
    std::unique_ptr<CostFunction> get_cost_function();
    std::unique_ptr<TreeWriter> get_tree_writer();
};

#endif /* defined(__tealtree__workflow__) */

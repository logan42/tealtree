#include "dense_feature.h"
#include "evaluator.h"
#include "fast_sparse_feature.h"
#include "gheap.h"
#include "log_trivial.h"
#include "metric.h"
#include "ranking_cost_function.h"
#include "regression_cost_function.h"
#include "sparse_feature.h"
#include "util.h"
#include "workflow.h"

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <random>
#include <thread>

class InMemoryBucketsProvider : public BucketsProvider
{
private:
    Workflow * workflow;
public:
    InMemoryBucketsProvider(Workflow * workflow) {
        this->workflow = workflow;
    }
    virtual BucketsCollection * get_buckets(uint32_t feature_id) {
        return workflow->get_trainer()->get_feature(feature_id)->get_buckets();
    }
};


class ColumnConsumerProviderForTraining : public ColumnConsumerProvider
{
public:
    RawFeatureType default_type;
    std::unique_ptr<RawFeature<float_t>> label_consumer;
    std::unique_ptr<QueryColumnConsumer> query_consumer;
    std::vector<std::unique_ptr<DynamicRawFeature>> features;

    virtual ColumnConsumer* create_label_consumer()
    {
        assert(label_consumer.get() == nullptr);
        label_consumer = std::unique_ptr<RawFeature<float_t>>(new RawFeature<float_t>());
        return label_consumer.get();
    }
    virtual ColumnConsumer*create_query_consumer()
    {
        assert(query_consumer.get() == nullptr);
        query_consumer = std::unique_ptr<QueryColumnConsumer>(new QueryColumnConsumer());
        return query_consumer.get();
    }
    virtual ColumnConsumer* create_feature_consumer()
    {
        std::unique_ptr<DynamicRawFeature> drf(new DynamicRawFeature(0, this->default_type));
        DynamicRawFeature * result = drf.get();
        features.push_back(std::move(drf));
        return result;
    }
};


Workflow::Workflow(Options & options) : options(options)
{
}

void Workflow::run()
{
    this->init();
    if (this->options.action == "train") {
        this->run_train();
        return;
    }
    if (this->options.action == "evaluate") {
        this->run_evaluate();
        return;
    }
    throw std::runtime_error("Unknown action.");
}

void Workflow::run_train()
{
this->tree_writer = this->get_tree_writer();
    this->cost_function = this->get_cost_function();
    this->tree_writer->set_cost_function(cost_function->get_registry_name());
    auto training_data = this->read_tsv();
    const std::vector<float_t> * labels = &std::get<0>(training_data);
    const std::vector<DOC_ID> * query_limits = &std::get<1>(training_data);
    FEATURE_PIPELINE_PTR_TYPE  features = std::move(std::get<2>(training_data));
    this->trainer = this->create_trainer(labels, query_limits, std::move(features));
    this->trainer->set_cost_function(std::move(cost_function));
    this->train_ensemble();
    this->tree_writer->close();
}

void Workflow::run_evaluate()
{
    this->ensemble = load_ensemble(this->options.input_tree);
    std::string metric_name = this->options.metric;
    if (metric_name.size() == 0) {
        metric_name = CostFunction::create(ensemble->get_cost_function())->get_default_metric_name();
    }
    std::unique_ptr<Metric> metric = std::unique_ptr<Metric>(Metric::get_metric(metric_name));

    INPUT_ROW_PIPELINE_PTR_TYPE input_pipe= std::shared_ptr<INPUT_ROW_PIPELINE_TYPE>(new INPUT_ROW_PIPELINE_TYPE(this->get_bbq_size()));
    std::shared_ptr<ColumnConsumerProvider> ccp(new ColumnConsumerProviderForEvaluation(input_pipe, ensemble.get(), this->options.exponentiate_label));
    std::unique_ptr<DataFileReader> tsv = this->get_tsv_reader(ccp, metric->is_query_based());

    std::thread reader(
         [tsv = std::move(tsv)]() mutable {
        tsv->read();
    });
    reader.detach();

    std::unique_ptr<std::ostream> epochs;
    if (this->options.output_epochs.size() > 0) {
        epochs = std::unique_ptr<std::ostream>(new std::ofstream(this->options.output_epochs));
    }
    std::unique_ptr<std::ostream> predictions;
    if (this->options.output_predictions.size() > 0) {
        predictions = std::unique_ptr<std::ostream>(new std::ofstream(this->options.output_predictions));
    }

    EVALUATED_ROW_PIPELINE_PTR_TYPE evaluated_pipe = std::shared_ptr<EVALUATED_ROW_PIPELINE_TYPE>(new EVALUATED_ROW_PIPELINE_TYPE(this->get_bbq_size()));
    Evaluator evaluator(ensemble.get(), input_pipe, evaluated_pipe, this->get_thread_pool(), epochs != nullptr);
    evaluator.evaluate_all();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    while (true) {
        std::unique_ptr<EvaluatedRow> row = evaluated_pipe->pop().get();
        if (row == nullptr) {
            break;
        }
        if (predictions != nullptr) {
            (*predictions) << row->scores[row->scores.size() - 1] << std::endl;
        }
        metric->consume_row(std::move(row));
    }

    std::cout << metric->get_name() << " = " << format_float(metric->get_metric_value(), 5, false) << std::endl;
    if (epochs != nullptr) {
        std::vector<float_t> ep = metric->get_epochs();
        for (size_t i = 0; i < ep.size(); i++) {
            (*epochs) << ep[i] << std::endl;
        }
    }
}

boost::threadpool::pool * Workflow::get_thread_pool()
{
    return this->thread_pool.get();
}

// Boost maintainers move this class from one header to another too often.
// We want to be compatible with multiple versions of boost, therefore we copy it here.
struct null_deleter
{
    //! Function object result type
    typedef void result_type;
    /*!
    * Does nothing
    */
    template< typename T >
    void operator() (T*) const BOOST_NOEXCEPT {}
};

void Workflow::init_logging()
{
    boost::log::core::get()->set_filter
        (
            boost::log::trivial::severity >= (boost::log::trivial::severity_level)options.logging_severity
            );

    // Construct the sink
    typedef boost::log::sinks::synchronous_sink< boost::log::sinks::text_ostream_backend > text_sink;
    boost::shared_ptr< text_sink > sink = boost::make_shared< text_sink >();

    // We have to provide an empty deleter to avoid destroying the global stream object
    boost::shared_ptr< std::ostream > stream(&std::clog, null_deleter());

    // Add a stream to write log to
    sink->locked_backend()->add_stream(stream);

    // Register the sink in the logging core
    boost::log::core::get()->add_sink(sink);
}

void Workflow::init_registries()
{
    Feature::registry.register_class<DenseFeatureImpl<1>>();
    Feature::registry.register_class<DenseFeatureImpl<2>>();
    Feature::registry.register_class<DenseFeatureImpl<4>>();
    Feature::registry.register_class<DenseFeatureImpl<8>>();
    Feature::registry.register_class<DenseFeatureImpl<16>>();

        FastShardMapping::get_instance().initial_tail_size = this->options.initial_tail_size;
        bool sparse_v1;
        if (options.sparse_feature_version == "v1") {
            sparse_v1 = true;
        }
        else if (options.sparse_feature_version == "v2") {
            sparse_v1 = false;
        }
        else if (options.sparse_feature_version == "auto") {
            sparse_v1 = options.n_leaves < 100;
        }
        else {
            throw std::runtime_error("Option sparse_feature_version has unknown value " + options.sparse_feature_version);
        }
        if (!sparse_v1) {
            BOOST_LOG_TRIVIAL(warning) << "Using sparse features V2.";
        }
        FastShardMapping::get_instance().sparse_v1 = sparse_v1;
        Feature::registry.register_class<FastSparseFeatureImpl<1>>();
        Feature::registry.register_class<FastSparseFeatureImpl<2>>();
        Feature::registry.register_class<FastSparseFeatureImpl<4>>();
        Feature::registry.register_class<FastSparseFeatureImpl<8>>();
        Feature::registry.register_class<FastSparseFeatureImpl<16>>();

    CostFunction::registry.register_class<SingleDocumentCostFunction<LinearRegressionStep>>();
    CostFunction::registry.register_class<SingleDocumentCostFunction<LogisticRegressionStep>>();
    CostFunction::registry.register_class<RankingCostFunction<LambdaRank>>();

    AbstractFeatureMetadataImpl::registry.register_class<FeatureMetadataImpl<uint8_t>>();
    AbstractFeatureMetadataImpl::registry.register_class<FeatureMetadataImpl<int8_t>>();
    AbstractFeatureMetadataImpl::registry.register_class<FeatureMetadataImpl<uint16_t>>();
    AbstractFeatureMetadataImpl::registry.register_class<FeatureMetadataImpl<int16_t>>();
    AbstractFeatureMetadataImpl::registry.register_class<FeatureMetadataImpl<uint32_t>>();
    AbstractFeatureMetadataImpl::registry.register_class<FeatureMetadataImpl<int32_t>>();
    AbstractFeatureMetadataImpl::registry.register_class<FeatureMetadataImpl<float_t>>();
}

void Workflow::init()
{
    this->init_logging();
    this->init_registries();
    uint32_t concurrency = this->get_concurrency();
    this->thread_pool = std::unique_ptr<boost::threadpool::pool>(new boost::threadpool::pool(concurrency));
    BOOST_LOG_TRIVIAL(info) << "ThreadPool initialized with " << concurrency << " threads.";
    if (this->options.random_seed == 0) {
        std::random_device rd;
        this->random_engine = std::unique_ptr<std::mt19937 >(new std::mt19937(rd()));
    } else {
        this->random_engine = std::unique_ptr<std::mt19937 >(new std::mt19937(this->options.random_seed));
    }
    
#ifndef NDEBUG
    BOOST_LOG_TRIVIAL(warning) << "TealTree is compiled with debug checks. Expect lower performance.";
#endif

    SplitLite::current_metadata = nullptr;
    init_DCG_cache(10000);
}

void Workflow::check_for_overflow()
{
    TrainerData * data = trainer->get_data();;
    float_t max_score = 0;
    for (size_t i = 0; i < data->documents.size(); i++) {
        max_score = std::max(max_score, std::abs(data->documents[i].score));
    }
        if (max_score > 1e12) {
            if (!this->msg_score_too_large) {
                BOOST_LOG_TRIVIAL(warning) << "Document scores are getting too large. For ranker this might indicate overfitting.";
                this->msg_score_too_large = true;
            }
        }
    }


uint32_t Workflow::get_concurrency()
{
    if (this->options.n_threads == 0)
        return std::thread::hardware_concurrency();
    else
        return this->options.n_threads;
}

uint32_t Workflow::get_bbq_size()
{
    return 2 * this->get_concurrency();
}

void Workflow::log_gradient()
{
    TrainerData * data = this->trainer->get_data();

    std::ostringstream oss;
    size_t n = std::min<size_t>(100, data->documents.size());
    for (size_t i = 0; i < n; i++) {
        if (i > 0) {
            oss << ", ";
        }
        oss << data->documents[i].gradient;
    }
    if (n < data->documents.size()) {
        oss << ", ...";
    }
    else {
        oss << ".";
    }
    BOOST_LOG_TRIVIAL(trace) << "Computed gradients: " << oss.str();
    if (this->options.step == "newton") {
        oss.clear();
        for (size_t i = 0; i < n; i++) {
            if (i > 0) {
                oss << ", ";
            }
            oss << data->documents[i].hessian;
        }
        if (n < data->documents.size()) {
            oss << ", ...";
        }
        else {
            oss << ".";
        }
        BOOST_LOG_TRIVIAL(trace) << "Computed hessians: " << oss.str();
    }
}

void Workflow::train_ensemble()
{
    BOOST_LOG_TRIVIAL(info) << "Training started ...";
    this->trainer->start_ensemble();
    this->set_base_score();
    for (uint32_t tree_index = 0; tree_index < options.n_trees; tree_index++) {
        this->train_a_tree(tree_index);
    }
    BOOST_LOG_TRIVIAL(info) << "Training finished.";
}

void Workflow::set_base_score()
{
    if (this->options.base_score == (float_t)0.0) {
        return;
    }
    this->trainer->start_new_tree();
    this->trainer->set_base_score(this->options.base_score);
    TrainerData * data = trainer->get_data();
    TreeLite tree(*data->current_tree, this->buckets_provider.get());
    this->trainer->clear_tree();
    this->tree_writer->add_tree(tree);
    this->check_for_overflow();
}

void Workflow::train_a_tree(uint32_t tree_index)
{
    assert(this->trainer != NULL);
    typedef gheap<> gh;
    CompareSplits compare;
    std::vector<Split*> heap;
    TIMER_START(t);
    uint32_t n_tree_nodes = 2 * options.n_leaves - 1;
    float_t step_alpha = options.step_alpha;
    TrainerData * data = trainer->get_data();

    this->trainer->start_new_tree();
    if (this->options.logging_severity <= 0) {
        this->log_gradient();
}
    TreeNode * root = data->current_tree->get_root();
    this->trainer->compute_histograms(root, nullptr, nullptr);
    if (root->split->spread > 0) {
        heap.push_back(root->split.get());
        gh::push_heap(heap.begin(), heap.end(), compare);
    }
    else {
        BOOST_LOG_TRIVIAL(warning) << "Warning: cannot split root in this tree. This might indicate overfitting.";

    }
    while (data->current_tree->get_nodes().size() < n_tree_nodes) {
        if (heap.empty()) {
            if (!this->msg_tree_too_short) {
                this->msg_tree_too_short = true;
                BOOST_LOG_TRIVIAL(warning) << "Terminating tree before max leaves reached.";
            }
            break;
        }
        Split * best_split = heap[0];
        gh::pop_heap(heap.begin(), heap.end(), compare);
        heap.pop_back();
        std::unique_ptr<SplitSignature> signature = this->trainer->get_split_signature(best_split);
        BOOST_LOG_TRIVIAL(trace) << "Node #" << best_split->node->node_id << " has " << best_split->node->doc_ids.size() << " docs, splitting by feature " << best_split->feature->get_name() << " inverse=" << best_split->inverse << " bucket=" << best_split->threshold;

        bool compute_children_histograms = (this->options.max_depth == 0) || (best_split->node->get_depth() + 1 < this->options.max_depth);
        std::pair<TreeNode*, TreeNode*> children = this->trainer->split_node(best_split->node, signature.get(), compute_children_histograms);
        BOOST_LOG_TRIVIAL(trace) << "Left Node #" << children.first->node_id << " has " << children.first->doc_ids.size() << " docs, Right Node #" << children.second->node_id << " has " << children.second->doc_ids.size();
        if ((children.first->doc_ids.size() == 0) || (children.second->doc_ids.size() == 0)) {
            throw std::runtime_error("Something went wrong. Either left or right child has 0 documents. This is not supposed to happen.");
        }
        assert(children.first->doc_ids.size() > 0);
        assert(children.second->doc_ids.size() > 0);
        if (this->options.step != "newton") {
            assert(children.first->doc_ids.size() >= this->options.min_node_docs);
            assert(children.second->doc_ids.size() >= this->options.min_node_docs);
        }

        if (compute_children_histograms) {
        this->trainer->compute_histograms(children.second, children.first, std::move(signature));
        if (children.first->split->spread > 0) {
                heap.push_back(children.first->split.get());
                gh::push_heap(heap.begin(), heap.end(), compare);
        }
        if (children.second->split->spread > 0) {
            heap.push_back(children.second->split.get());
            gh::push_heap(heap.begin(), heap.end(), compare);
        }
        }

        root = children.first;
    }
    this->trainer->finalize_tree(step_alpha);
    TreeLite tree(*data->current_tree, this->buckets_provider.get());
    this->trainer->clear_tree();
    
    BOOST_LOG_TRIVIAL(info) << "Tree #" << tree_index << " trained in " << format_float(TIMER_FINISH(t), 3) << " seconds.";
    
    this->tree_writer->add_tree(tree);

    this->check_for_overflow();
}

std::unique_ptr<std::vector<std::string>> Workflow::get_feature_names()
{
    if (this->options.feature_names_file.size() == 0) {
        if (this->ensemble != nullptr) {
            std::unique_ptr<std::vector<std::string>> result(new std::vector<std::string>());
            for (size_t i = 0; i < this->ensemble->get_features().size(); i++) {
                result->push_back(this->ensemble->get_features()[i].get_name());
            }
            return result;
        }
        return nullptr;
    }
    std::unique_ptr<std::vector<std::string>> result(new std::vector<std::string>());
    std::ifstream fin(this->options.feature_names_file);
    std::string line;
    while (std::getline(fin, line)) {
        result->push_back(line);
    }

    return result;
}

std::pair<std::unique_ptr<LineReader>, std::string> Workflow::get_line_reader()
{
    if (this->options.input_file.size() > 0) {
        return std::make_pair(
            std::unique_ptr<LineReader>(new FileReader(this->options.input_file.c_str())),
            this->options.input_file);
    }
    else if (this->options.input_pipe.size() > 0) {
        return std::make_pair(
            std::unique_ptr<LineReader>(new PipeReader(this->options.input_pipe.c_str())),
            "pipe");
    }
    else {
        return std::make_pair(
            std::unique_ptr<LineReader>(new StdInReader()),
            "stdin");
    }
    throw std::runtime_error("Canot create training data input source.");
}


std::unique_ptr<DataFileReader> Workflow::get_tsv_reader(std::shared_ptr<ColumnConsumerProvider> ccp, bool with_query)
{
    auto pair = std::move(this->get_line_reader());
    std::unique_ptr<LineReader>line_reader = std::move(pair.first);
    std::string input_source = pair.second;

    std::unique_ptr<DataFileReader> result;
    if (options.input_format == "tsv") {
        std::string tsv_query = with_query ? this->options.tsv_query : "";
    DataFileReader * tsv = new TsvReader(std::move(line_reader), this->options.tsv_separator, ccp, tsv_query, this->options.tsv_label);
    result = std::unique_ptr<DataFileReader>(tsv);
    }
    if (options.input_format == "svm") {
        std::string svm_query = with_query ? this->options.svm_query : "";
        SvmReader * svm = new SvmReader(std::move(line_reader), ccp, svm_query);
        std::unique_ptr<std::vector<std::string>> feature_names = this->get_feature_names();
        if (feature_names.get() != nullptr) {
            svm->set_feature_names(std::move(feature_names));
        }
        result = std::unique_ptr<DataFileReader>(svm);
    }
    if (result.get() == nullptr) {
        throw std::runtime_error("Cannot create file format parser.");
    }
    result->set_sample_rate(this->options.input_sample_rate, this->random_engine.get(), false);
    std::string sample_rate_clause;
    if (this->options.input_sample_rate < 1.0) {
        sample_rate_clause = " with " + format_float(this->options.input_sample_rate, 3) + " subsample rate";
    }
    BOOST_LOG_TRIVIAL(info) << "Reading data from " << input_source << " in " << this->options.input_format << " format" << sample_rate_clause << " ...";
    return result;
}

std::tuple<const std::vector<float_t>, const std::vector<DOC_ID>, Workflow::FEATURE_PIPELINE_PTR_TYPE  > Workflow::read_tsv()
{
    ColumnConsumerProviderForTraining * ccp_plain_ptr = new ColumnConsumerProviderForTraining();
    std::shared_ptr<ColumnConsumerProviderForTraining >  ccp_ptr (ccp_plain_ptr);
    ccp_ptr->default_type = RawFeatureType(this->options.default_raw_feature_type);
    std::unique_ptr<DataFileReader> tsv = this->get_tsv_reader(ccp_ptr, this->cost_function->is_query_based());
    tsv->read();
    BOOST_LOG_TRIVIAL(info)
        << "Loaded " << ccp_ptr->label_consumer->get_data()->size() << " documents, "
        << (ccp_ptr->query_consumer->get_query_limits()->size() - 1) << " queries and "
        << ccp_ptr->features.size() << " features.";
    BOOST_LOG_TRIVIAL(info) << "Cooking features...";
    std::unique_ptr<std::vector<float_t>> labels(new std::vector<float_t>(*ccp_ptr->label_consumer->get_data()));
    std::unique_ptr<std::vector<float_t>>  labels2 = this->preprocess_labels(std::move(labels));
    return std::make_tuple(
        *labels2,
        *ccp_ptr->query_consumer->get_query_limits(),
        this->cook_features(std::move(ccp_ptr->features))
        );
}

std::unique_ptr<Feature>  Workflow::cook_feature(std::unique_ptr<DynamicRawFeature> drf)
{
    std::unique_ptr<Feature> feature;
    
    std::unique_ptr<RawFeatureHistogram> hist = drf->to_histogram();
    std::string feature_name(*(drf->get_name()));
    drf.reset();
    feature = create_feature_from_histogram(hist.get(), options.sparsity_threshold);
    feature->set_name(feature_name);
    return std::move(feature);
}

std::unique_ptr<std::vector<float_t>> Workflow::preprocess_labels(std::unique_ptr<std::vector<float_t>> labels)
{
    if (!this->options.exponentiate_label) {
        return std::move(labels);
    }
    std::unique_ptr<std::vector<float_t>> result(new std::vector<float_t>(labels->size()));
    for (size_t i = 0; i < labels->size(); i++) {
        (*result)[i] = (float_t) pow(2, (*labels)[i]) - 1;
    }
    return result;
}

void Workflow::log_feature_types(std::map<std::string, size_t> & feature_types, char prefix, std::string msg_prefix)
{
    std::ostringstream oss;
    const static std::vector<uint32_t> bits = { 1,2,4,8,16 };
    for (size_t i = 0; i < bits.size(); i++) {
        if (i > 0) {
            oss << ", ";
        }
        oss << prefix << bits[i] << ":" << feature_types[prefix + std::to_string(bits[i])];
    }
    BOOST_LOG_TRIVIAL(info) << msg_prefix << oss.str() << ".";
}

Workflow::FEATURE_PIPELINE_PTR_TYPE  Workflow::cook_features(std::vector<std::unique_ptr<DynamicRawFeature>> drfs)
{
    size_t n_features = drfs.size();
    FEATURE_PIPELINE_PTR_TYPE bbq(new FEATURE_PIPELINE_TYPE(this->get_bbq_size()));
    auto tp = this->get_thread_pool();
    
    std::thread starter_thread(
        [this, n_features, drfs = std::move(drfs), bbq, tp]()mutable {
        for (size_t i = 0; i < n_features; i++) {
            std::unique_ptr<DynamicRawFeature> drf = std::move(drfs[i]);
            std::promise<std::unique_ptr<Feature>> promise;
            bbq->push(promise.get_future());

            std::function<void ()>function = make_copyable_function<void()>(
                [this, drf = std::move(drf), promise = std::move(promise)]() mutable {
                try {
                    std::unique_ptr<Feature> result = std::move(this->cook_feature(std::move(drf)));
                    promise.set_value(std::move(result));
                }
                catch (...) {
                    promise.set_exception(std::current_exception());
                }
            });
            /*
            // My threadpool sucks, I wish it supported packaged tasks
            std::packaged_task<std::unique_ptr<Feature>()> task(function);
            std::future<std::unique_ptr<Feature>> future = task.get_future();
            bbq->push(std::move(future));
            */
            tp->schedule(function);
        }
        std::promise<std::unique_ptr<Feature>> promise;
        bbq->push(promise.get_future());
        promise.set_value(nullptr);
    });
starter_thread.detach();

return bbq;
}


std::unique_ptr<Trainer> Workflow::create_trainer(const std::vector<float_t> * labels, const std::vector<DOC_ID> * query_limits, FEATURE_PIPELINE_PTR_TYPE   features)
{
    std::unique_ptr<Trainer> trainer(new Trainer());
    trainer->load_documents(labels, query_limits);
    std::map<std::string, size_t> feature_types;
    while(true) {
        std::unique_ptr<Feature> feature =  features->pop().get();
        if (feature.get() == nullptr) {
            break;
        }
        feature_types[feature->get_registry_name()]++;
        this->tree_writer->add_feature(std::move(feature->create_metadata()));
        trainer->add_feature(std::move(feature));
    }
    this->log_feature_types(feature_types, 'd', "Dense features encodings: ");
    this->log_feature_types(feature_types, 's', "Sparse features encodings: ");

    trainer->set_thread_pool(this->thread_pool.get());
    TrainerParams params;
    params.newton_step = this->options.step == "newton";
    params.quadratic_spread = this->options.quadratic_spread;
    params.regularization_lambda = this->options.regularization_lambda;
    params.min_node_docs = this->options.min_node_docs;
    params.min_node_hessian = this->options.min_node_hessian;
        params.tree_debug_info = this->options.tree_debug_info;
        
        trainer->set_parameters(params);
    this->buckets_provider = std::unique_ptr<BucketsProvider>(new InMemoryBucketsProvider(this));
    return trainer;
}

Trainer * Workflow::get_trainer()
{
    return this->trainer.get();
}


std::unique_ptr<CostFunction> Workflow::get_cost_function()
{
    std::unique_ptr<CostFunction> cf;
    cf = std::unique_ptr<CostFunction>(CostFunction::create(options.cost_function));
    if (cf.get() == nullptr) {
        throw std::runtime_error("Cannot create cost function.");
    }
    return cf;
}


std::unique_ptr<TreeWriter> Workflow::get_tree_writer()
{
    std::unique_ptr<TreeWriter> result(new TreeWriter(this->options.output_tree));
    return result;
}


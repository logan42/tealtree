#ifndef __tealtree__EVALUATOR__
#define __tealtree__EVALUATOR__

#include <boost/algorithm/string/join.hpp>
#include <future>
#include <stdio.h>
#include <vector>

#include "blocking_queue.h"
#include "column_consumer.h"
#include "cost_function.h"
#include "log_trivial.h"
#include "split.h"
#include "thread_pool.h"
#include "tree.h"
#include "tree_node.h"
#include "types.h"

typedef std::vector<FeatureValue> FEATURE_VECTOR_TYPE;

struct  InputRow
{
    DOC_ID doc_id;
    float_t label;
    std::string query;
    FEATURE_VECTOR_TYPE features;
};

typedef BlockingBoundedQueue<std::unique_ptr<InputRow>> INPUT_ROW_PIPELINE_TYPE;
typedef std::shared_ptr<INPUT_ROW_PIPELINE_TYPE> INPUT_ROW_PIPELINE_PTR_TYPE;

struct  EvaluatedRow
{
    float_t label;
    std::string query;
    std::vector<float_t> scores;
};


typedef BlockingBoundedQueue<std::future<std::unique_ptr<EvaluatedRow>>> EVALUATED_ROW_PIPELINE_TYPE;
typedef std::shared_ptr<EVALUATED_ROW_PIPELINE_TYPE> EVALUATED_ROW_PIPELINE_PTR_TYPE;


class ColumnConsumerProviderForEvaluation : public ColumnConsumerProvider
{
private:
    INPUT_ROW_PIPELINE_PTR_TYPE pipeline;
    Ensemble * ensemble;
    bool exponentiate_label;
    InputRow row;
    std::unique_ptr<ColumnConsumer> label_consumer, query_consumer;
    std::vector<std::unique_ptr<ColumnConsumer>> feature_consumers;
    void reset_row() 
    {
        this->row.label = 0;
        this->row.query = "";
        this->row.features.resize(0);
        this->row.features.resize(this->ensemble->get_features().size(), FeatureValue());
    }
public:
    ColumnConsumerProviderForEvaluation(INPUT_ROW_PIPELINE_PTR_TYPE pipeline, Ensemble * ensemble, bool exponentiate_label)
        : pipeline(pipeline),
        ensemble(ensemble),
        exponentiate_label(exponentiate_label)
    {
        this->reset_row();
    }
    virtual ColumnConsumer* create_label_consumer()
    {
        class EvalLabelConsumer : public ColumnConsumer 
        {
        private:
            float_t & value;
            bool exponentiate_label;
        public:
            EvalLabelConsumer(float_t &value, bool exponentiate_label) 
                : value(value),
                exponentiate_label(exponentiate_label)
            {}
            virtual void consume_cell(const char * value)
            {
                this->value = parse_string<float_t>(value);
                if (exponentiate_label) {
                    this->value = (float_t)pow((float_t)2, this->value) - 1;
                }
            }
        };
        auto cc = new EvalLabelConsumer(this->row.label, this->exponentiate_label);
        assert(this->label_consumer == nullptr);
        this->label_consumer =  std::unique_ptr<ColumnConsumer>(cc);
        return cc;
    }
    virtual ColumnConsumer * create_query_consumer()
    {
        class EvalQueryConsumer : public ColumnConsumer
        {
        private:
            std::string& query;
        public:
            EvalQueryConsumer(std::string&query) : query(query) {}
            virtual void consume_cell(const char * value)
            {
                this->query = value;
            }
        };
        auto cc = new EvalQueryConsumer(this->row.query);
        assert(this->query_consumer== nullptr);
        this->query_consumer = std::unique_ptr<ColumnConsumer>(cc);
        return cc;
    }
    virtual ColumnConsumer* create_feature_consumer()
    {
        class EvalFeatureConsumer : public ColumnConsumer
        {
        private:
            FeatureValue & value;
            const FeatureMetadata & metadata;
        public:
            EvalFeatureConsumer(FeatureValue&value, const FeatureMetadata & metadata) : value(value), metadata(metadata) {}
            virtual void consume_cell(const char * value)
            {
                this->value = metadata.string_to_value(value);
            }
            virtual void consume_cell(const char * value, DOC_ID doc_id)
            {
                try {
                return this->consume_cell(value);
            }
            catch (number_format_error& e) {
                this->report_exception("number_format_error", e, value, doc_id);
            }
            throw std::runtime_error("Not supposed to reach here.");
            }
            void report_exception(const std::string & exception_name, std::exception & e, const char * value, DOC_ID doc_id)
            {
                logger->critical("Exception {} was thrown while trying to parse"
                    " value='{}' as type {}, doc_id={}, feature={}",
                    exception_name, value, this->metadata.get_type(), doc_id, this->metadata.get_name());
                logger->critical("This can sometimes happen if a particular feature in the training data fits for example into an 8-bit integer, "
                    "but in the testing data it happens to have a larger value, causing an overflow.");
                logger->critical("You can work around this by setting option --default_raw_feature_type to a higher value during training, "
                    "or alternatively, you can edit the ensemble json file and manually change the type "
                    "of the affected feature to a larger size integer type.");
                throw e;
            }
                        virtual void set_name(const char * name)
            {
                if (this->metadata.get_name() != name) {
                    throw std::runtime_error("Mismatched feature name. Expected: " + this->metadata.get_name() + ", but actually got: " + name);
                }
            }
        };
        size_t i = this->feature_consumers.size();
        auto cc = new EvalFeatureConsumer(this->row.features[i], ensemble->get_features()[i]);
        this->feature_consumers.push_back(std::unique_ptr<ColumnConsumer>(cc));
        return cc;
    }
    virtual void on_end_of_row(DOC_ID doc_id) 
    {
        // make a copy to return
        std::unique_ptr<InputRow> result(new InputRow(this->row));
        this->reset_row();
        result->doc_id = doc_id;
        this->pipeline->push(std::move(result));
    }
    virtual void on_end_of_file(DOC_ID n_docs)
    {
        this->pipeline->push(nullptr);
    }

};

class Evaluator
{
private:
    Ensemble * ensemble;
    INPUT_ROW_PIPELINE_PTR_TYPE input;
    EVALUATED_ROW_PIPELINE_PTR_TYPE output;
    ThreadPool * tp;
    bool all_epochs;
    std::unique_ptr<CostFunction> cost_function;
public:
    Evaluator(Ensemble * ensemble, INPUT_ROW_PIPELINE_PTR_TYPE input, EVALUATED_ROW_PIPELINE_PTR_TYPE output, ThreadPool * tp, bool all_epochs)
        : ensemble(ensemble),
        input(input),
        output(output),
        tp(tp),
        all_epochs(all_epochs)
    {
        cost_function = std::unique_ptr<CostFunction>(CostFunction::create(ensemble->get_cost_function()));
    }

    void evaluate_all()
    {
        async_fill_pipeline(this->output, [=]()
        {
            std::unique_ptr<InputRow> input_row;
            while ((input_row = this->input->pop()) != nullptr) {
                this->output->push(tp->enqueue(true, make_copyable_function<std::unique_ptr<EvaluatedRow>()>(
                    [this, input_row= std::move(input_row)]() mutable {
                        return this->evaluate_ensemble(std::move(input_row));
                })));
            }
            std::promise<std::unique_ptr<EvaluatedRow>> promise;
            this->output->push(promise.get_future());
            promise.set_value(nullptr);
        });
    }
private:     float_t evaluate_tree(const TreeLite & tree, const std::vector<FeatureValue> & values)
    {
        assert(tree.get_nodes().size() > 0);
        size_t current_node_id = 0;
        while (!std::isfinite(tree.get_nodes()[current_node_id].value)) {
            const TreeNodeLite & node = tree.get_nodes()[current_node_id];
            const SplitLite & split = node.split;
            size_t feature_id = split.feature;
            const FeatureMetadata & meta = ensemble->get_features()[feature_id];
            bool condition = meta.is_greater_or_equal(values[feature_id], split.threshold);
            if (split.inverse) {
                condition = !condition;
            }
            if (condition) {
                current_node_id = node.right_id;
            }
            else {
                current_node_id = node.left_id;
            }
            assert(current_node_id < tree.get_nodes().size());
        }
        return tree.get_nodes()[current_node_id].value;
    }

    std::unique_ptr<EvaluatedRow> evaluate_ensemble(std::unique_ptr<InputRow> input_row)
    {
        std::unique_ptr<EvaluatedRow> result(new EvaluatedRow());
        result->label = input_row->label;
        result->query = input_row->query;
        std::vector<float_t> scores(this->ensemble->get_trees().size());
        for (size_t i = 0; i < this->ensemble->get_trees().size(); i++) {
            scores[i] = this->evaluate_tree(this->ensemble->get_trees()[i], input_row->features);
        }
        for (size_t i = 1; i < this->ensemble->get_trees().size(); i++) {
                scores[i] += scores[i - 1];
        }
        this->cost_function->transform_scores(scores);
        if (this->all_epochs) {
            result->scores = scores;
        }
        else {
            result->scores.push_back(scores[scores.size()-1]);
        }
        return result;
    }
};

#endif /* defined(__tealtree__EVALUATOR__) */

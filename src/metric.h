#ifndef __tealtree__METRIC__
#define __tealtree__METRIC__

#include <memory>
#include <stdio.h>
#include <vector>

#include "evaluator.h"
#include "log_trivial.h"

class Metric
{
public:
    static std::unique_ptr<Metric> get_metric(const std::string & name);

    virtual ~Metric() {}
    virtual float_t get_metric_value()
    {
        std::vector<float_t> epochs = this->get_epochs();
        return epochs[epochs.size() - 1];
    }

    virtual void consume_row(std::unique_ptr<EvaluatedRow> row) = 0;
    virtual std::vector<float_t> get_epochs() = 0;
    virtual std::string get_name() = 0;
    virtual bool is_query_based() 
    { 
        return false; 
    }
};

class AveragingMetric : public Metric
{
private:
    std::vector<float_t> epochs;
    DOC_ID count;
protected:
    AveragingMetric()
        :epochs(0),
        count(0)
        {}

    virtual void consume_row(std::vector<float_t> & epochs)
    {
        if (this->epochs.size() == 0) {
            assert(epochs.size() > 0);
            this->epochs.resize(epochs.size());
        }
        else {
            assert(this->epochs.size() == epochs.size());
        }
        for (size_t i = 0; i < epochs.size(); i++) {
            this->epochs[i] += epochs[i];
        }
        this->count++;
    }
public:
    virtual std::vector<float_t> get_epochs()
    {
        std::vector<float_t> result(this->epochs);
        for (size_t i = 0; i < result.size(); i++) {
            result[i] /= this->count;
        }
        return result;
    }
};

class RMSEMetric : public AveragingMetric
{
public:
    virtual void consume_row(std::unique_ptr<EvaluatedRow> row)
    {
        std::vector<float_t> errors(row->scores.size());
        for (size_t i = 0; i < errors.size(); i++) {
            float_t error = row->scores[i] - row->label;
            errors[i] = error*error;
        }
        AveragingMetric::consume_row(errors);
    }
    virtual std::vector<float_t> get_epochs()
    {
        std::vector<float_t> epochs = AveragingMetric::get_epochs();
        for (size_t i = 0; i < epochs.size(); i++) {
            epochs[i] = sqrt(epochs[i]);
        }
        return epochs;

    }
    virtual std::string get_name()
    {
        return "RMSE";
    }
};

class AccuracyMetric: public AveragingMetric
{
public:
    virtual void consume_row(std::unique_ptr<EvaluatedRow> row)
    {
        std::vector<float_t> errors(row->scores.size());
        for (size_t i = 0; i < errors.size(); i++) {
            bool correct = (row->scores[i] >= 0.5) == (row->label >= 0.5);
            errors[i] = correct ? 1.0f : 0.0f;
        }
        AveragingMetric::consume_row(errors);
    }

    virtual std::string get_name()
    {
        return "Accuracy";
    }
};


struct EvaluatedQuery
{
    std::vector<float_t> labels;
    std::vector<std::vector<float_t>> scores;

    void transpose_scores()
    {
        assert(scores.size() > 0);
        assert(scores[0].size() > 0);
        for (size_t i = 0; i < scores.size(); i++) {
            assert(scores[i].size() == scores[0].size());
        }
        std::vector<std::vector<float_t>> transposed(scores[0].size());
        for (size_t i = 0; i < transposed.size(); i++) {
            transposed[i].resize(scores.size());
            for (size_t j = 0; j < scores.size(); j++) {
                transposed[i][j] = scores[j][i];
            }
        }
        scores.swap(transposed);
    }
};

class QueryBasedMetric : public AveragingMetric
{
private:
    std::string last_query;
    std::unique_ptr<EvaluatedQuery> query;

    void flush()
    {
        if (this->query != nullptr) {
            this->query->transpose_scores();
            this->consume_query(std::move(this->query));
        }
        this->query = nullptr;
    }
public:
    virtual void consume_query(std::unique_ptr<EvaluatedQuery> query) = 0;

    virtual void consume_row(std::unique_ptr<EvaluatedRow> row)
    {
        if (row->query != this->last_query) {
            this->flush();
            this->last_query = row->query;
            this->query = std::unique_ptr<EvaluatedQuery>(new EvaluatedQuery());
        }
        this->query->labels.push_back(row->label);
        this->query->scores.push_back(row->scores);
    }

    virtual std::vector<float_t> get_epochs()
    {
        this->flush();
        return AveragingMetric::get_epochs();
    }
    virtual bool is_query_based()
    {
        return true;
    }
};

class NDCGMetric : public QueryBasedMetric
{
private:
    uint32_t depth;
public:
    NDCGMetric(uint32_t depth)
        :depth(depth)
    {}

        virtual void consume_query(std::unique_ptr<EvaluatedQuery> query)
    {
        std::vector<float_t> & labels = query->labels;
        std::vector<DOC_ID> order(labels.size());
        for (DOC_ID i = 0; i < order.size(); i++) {
            order[i] = i;
        }
        std::stable_sort(order.begin(), order.end(), [&labels](DOC_ID doc_id1, DOC_ID doc_id2) {
            return labels[doc_id1] > labels[doc_id2];
        });
        float_t idcg = this->calculate_dcg(labels, order);

        std::vector<float_t> ndcgs(query->scores.size());
        for (size_t epoch = 0; epoch < ndcgs.size(); epoch++) {
            std::vector<float_t> & scores = query->scores[epoch];
            for (DOC_ID i = 0; i < order.size(); i++) {
                order[i] = i;
            }

            std::stable_sort(order.begin(), order.end(), [&scores](DOC_ID doc_id1, DOC_ID doc_id2) {
                return scores[doc_id1] > scores[doc_id2];
            });
            float_t dcg = this->calculate_dcg(labels, order);
            float_t ndcg = (idcg > 0) ? dcg / idcg : (float_t)0;
            ndcgs[epoch] = ndcg;
        }
        AveragingMetric::consume_row(ndcgs);
    }

    virtual std::string get_name()
    {
        if (this->depth == 0) {
            return "NDCG";
        }
        return "NDCG@" + std::to_string(this->depth);
    }
private:
    float_t calculate_dcg(const std::vector<float_t> & scores, const std::vector<DOC_ID> & order)
    {
        size_t this_depth = this->depth;
        if (this_depth == 0) {
            this_depth = order.size();
        }
        this_depth = std::min(this_depth, order.size());
        float_t result = 0;
        for (size_t i = 0; i < this_depth; i++) {
            result += get_dcg_coefficient(i) * scores[order[i]];
        }
        return result;
    }
};


#endif /* defined(__tealtree__METRIC__) */

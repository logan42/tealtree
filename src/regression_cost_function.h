#ifndef __tealtree__regression_cost_function__
#define __tealtree__regression_cost_function__

#include <stdio.h>

#include "cost_function.h"
#include "util.h"

class LinearRegressionStep
{
public:
    static const std::string get_registry_name()
    {
        return "regression";
    }

    static inline float_t get_gradient(float_t score, float_t target_score)
    {
        return score - target_score;
    }
    static inline float_t get_hessian(float_t score, float_t target_score)
    {
        return 1.0;
    }

    static inline float_t transform_score(float_t score)
    {
        return score;
    }

    static std::string get_default_metric_name()
    {
        return "rmse";
    }
};

class LogisticRegressionStep
{
public:
    static const std::string get_registry_name()
    {
        return "binary_classification";
    }

    static inline float_t get_gradient(float_t score, float_t target)
    {
        return sigmoid(score) - target;
    }
    static inline float_t get_hessian(float_t score, float_t target)
    {
        return std::max(EPSILON, sigmoid_prime(score));
    }

    static inline float_t transform_score(float_t score)
    {
        return sigmoid(score);
    }

    static std::string get_default_metric_name()
    {
        return "accuracy";
    }
};


template <class T>
class SingleDocumentCostFunction : public CostFunction
{
public:
    virtual const std::string get_registry_name()
    {
        return T::get_registry_name();
    }
    virtual void compute_gradient(TrainerData * trainer_data, bool newton_step)
    {
        std::vector<Document> & documents = trainer_data->documents;
        for (size_t i = 0; i < documents.size(); i++) {
            documents[i].gradient = T::get_gradient(documents[i].score, documents[i].target_score);
            if (newton_step) {
                documents[i].hessian = T::get_hessian(documents[i].score, documents[i].target_score);
            }
        }
    }

    virtual void transform_scores(std::vector<float_t> & scores)
    {
        for (size_t i = 0; i < scores.size(); i++) {
            scores[i] = T::transform_score(scores[i]);
        }
    }

    virtual std::string get_default_metric_name()
    {
        return T::get_default_metric_name();
    }
};

#endif /* defined(__tealtree__regression_cost_function__) */

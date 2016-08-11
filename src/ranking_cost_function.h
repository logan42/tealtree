#ifndef __tealtree__RANKING_cost_function__
#define __tealtree__RANKING_cost_function__

#include <boost/thread/tss.hpp>
#include <stdio.h>
#include <vector>

#include "cost_function.h"
#include "log_trivial.h"
#include "util.h"


class LambdaRank
{
private:
    std::vector<DOC_ID> buffer;
    std::vector<DOC_ID> buffer2;
public:
    const std::string get_registry_name()
    {
        return "lambda_rank";
    }

    void precompute(TrainerData * trainer_data, DOC_ID ndcg_at);
    void compute_gradient_for_query(TrainerData * trainer_data, DOC_ID ndcg_at, DOC_ID query_index, bool newtonStep);

    LambdaRank()
    {}
    ~LambdaRank()
    {}
};

template <typename T>
class RankingCostFunction : public CostFunction
{
private:
    DOC_ID depth;
public:
    RankingCostFunction(DOC_ID depth = 0)
        :depth(depth)
    {}

    virtual const std::string get_registry_name()
    {
        return T().get_registry_name();
    }

    virtual bool is_query_based()
    {
        return true;
    }

    virtual void compute_gradient(TrainerData * trainer_data, bool newton_step);
    virtual void compute_gradient(TrainerData * trainer_data, bool newton_step, boost::threadpool::pool* tp);;

    virtual void precompute(TrainerData * trainer_data) 
    {
        T cf;
        cf.precompute(trainer_data, this->depth);
    }

    virtual std::string get_default_metric_name()
    {
        if (this->depth == 0) {
        return "ndcg";
        }
        else {
            return "ndcg@" + std::to_string(this->depth);
        }
    }
};

#endif /* defined(__tealtree__RANKING_cost_function__) */

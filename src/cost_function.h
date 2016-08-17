#ifndef __tealtree__cost_function__
#define __tealtree__cost_function__

#include <stdio.h>

#include "cost_function.h"
#include "thread_pool.h"
#include "trainer_data.h"
#include "registry.h"

class CostFunction
{
public:
    static Registry<CostFunction> registry;
    static std::unique_ptr<CostFunction> create(const std::string & class_name);
    virtual const std::string get_registry_name() = 0;

    virtual inline ~CostFunction() 
    {};
    virtual void precompute(TrainerData * trainer_data) {}
    virtual void compute_gradient(TrainerData * trainer_data, bool newton_step) = 0;
    virtual void compute_gradient(TrainerData * trainer_data, bool newton_step, ThreadPool * tp)
    {
        this->compute_gradient(trainer_data, newton_step);
    }
    virtual void transform_scores(std::vector<float_t> & scores) {}
    virtual std::string get_default_metric_name() = 0;
    virtual bool is_query_based() { return false; }
};

#endif /* defined(__tealtree__cost_function__) */

#include "cost_function.h"
#include "ranking_cost_function.h"

#include <boost/algorithm/string/predicate.hpp>

Registry<CostFunction> CostFunction::registry;

std::unique_ptr<CostFunction> CostFunction::create(const std::string & class_name)
{
    std::string ndcg_prefix = "lambda_rank@";
    if (boost::starts_with(class_name, ndcg_prefix)) {
        try {
            uint32_t depth = parse_string<uint32_t>(class_name.c_str() + ndcg_prefix.size());;
            return std::unique_ptr<CostFunction>(new RankingCostFunction<LambdaRank>(depth));
        }
        catch (number_format_error) {
            // Do nothing. exception will be thrown.
        }
    }

    return registry.create(class_name);
}

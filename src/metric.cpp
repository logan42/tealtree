#include "metric.h"

#include <boost/algorithm/string/predicate.hpp>

std::unique_ptr<Metric> Metric::get_metric(const std::string & name)
{
    if (name == "rmse") {
        return std::unique_ptr<Metric>(new RMSEMetric());
    }
    if (name == "accuracy") {
        return std::unique_ptr<Metric>(new AccuracyMetric());
    }
    if (name == "ndcg") {
        return std::unique_ptr<Metric>(new NDCGMetric(0));
    }
    std::string ndcg_prefix = "ndcg@";
    if (boost::starts_with(name, ndcg_prefix)) {
        try {
        uint32_t depth = parse_string<uint32_t>(name.c_str() + ndcg_prefix.size());;
        return std::unique_ptr<Metric>(new NDCGMetric(depth));
    }
    catch (number_format_error) {
        // Do nothing. exception will be thrown.
    }
    }

    throw std::runtime_error("Unknown metric: " + name);
}

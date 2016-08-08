#include <stdio.h>

#include "dense_feature.h"
#include "feature.h"
#include "trainer.h"

Registry<Feature> Feature::registry;

Feature::~Feature() { }

const std::string & Feature::get_name() const
{
    return this->name;
}

void Feature::set_name(std::string name)
{
    this->name = name;
}

FEATURE_INDEX Feature::get_index() const
{
    return this->index;
}

void Feature::set_index(FEATURE_INDEX index)
{
    this->index = index;
}

void Feature::set_trainer_data(TrainerData * trainer_data)
{
    this->trainer_data = trainer_data;
}

BucketsCollection * Feature::get_buckets()
{
    return this->buckets.get();
}

void Feature::set_buckets(std::unique_ptr<BucketsCollection> buckets)
{
    this->buckets = std::move(buckets);
}

FeatureMetadata Feature::create_metadata()
{
    FeatureMetadata result = this->buckets->create_metadata();
    result.set_name(this->name);
    return result;
}



const std::vector<uint8_t> feature_bits_list = { 1,2,4,8,16 };

std::unique_ptr<Feature> create_feature_from_histogram(const RawFeatureHistogram * hist, float_t sparsity_threshold)
{
    char          density_type = hist->get_sparsity() > sparsity_threshold ? 'd' : 's';;

    uint32_t n_buckets = hist->get_number_of_buckets();
    uint8_t bits = 0;
    for (size_t i = 0; i < feature_bits_list.size(); i++) {
        uint32_t max_buckets = 1u << feature_bits_list[i];
        if (n_buckets <= max_buckets) {
            bits = feature_bits_list[i];
            break;
        }
    }
    if (bits == 0) {
        throw std::runtime_error("Cannot create a feature object for a feature with " + std::to_string(n_buckets) + " buckets.");
    }
    std::unique_ptr<Feature>  result  = Feature::create(density_type + std::to_string((uint32_t)bits));
    result->init_from_raw_histogram(hist);
    return result;
}

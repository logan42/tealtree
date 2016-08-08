#include "options.h"
#include "raw_feature.h"
#include "raw_feature_histogram.h"
#include "util.h"


template <typename T>
RawFeature<T>::RawFeature(size_t expected_documents_count)
{
    this->data.reserve(expected_documents_count);
    this->min_element = std::numeric_limits<T>::max();
    this->max_element = std::numeric_limits<T>::min();
}

template <typename T>
RawFeature<T>::~RawFeature()
{
    
}

template <typename T>
void RawFeature<T>::consume_cell(const char * str_value)
{
    // TODO: maybe replace lexical_cast with atoi-like functions, for performance reasons.
    T value = parse_string<T>(str_value);
    this->min_element = std::min(this->min_element, value);
    this->max_element = std::max(this->max_element, value);
    this->data.push_back(value);
}

template <typename T>
void RawFeature<T>::set_next_docid(DOC_ID doc_id)
{
    assert(doc_id >= this->data.size());
    this->data.resize(doc_id, (T)0);
}


template <typename T>
std::string RawFeature<T>::get_min_str() const
{
    return to_string(this->min_element);
}

template <typename T>
std::string RawFeature<T>::get_max_str() const
{
    return to_string(this->max_element);
}

template <typename T>
std::unique_ptr<std::vector<std::string>> RawFeature<T>::export_data() const
{
    std::vector<std::string> * result = new std::vector<std::string>(this->data.size());
    for (size_t i=0; i<this->data.size(); i++) {
        (*result)[i] = to_string(this->data[i]);
    }
    return std::unique_ptr<std::vector<std::string>>(result);
}

template <typename T>
void RawFeature<T>::import_data(const std::vector<std::string> * import_data)
{
    assert(this->data.size() == 0);
    for (size_t i=0; i<import_data->size(); i++) {
        this->consume_cell((*import_data)[i].c_str());
    }
}

template <typename T>
const std::vector<T> * RawFeature<T>::get_data() const
{
    return &this->data;
}

template <typename T>
std::unique_ptr<RawFeatureHistogram> RawFeature<T>::to_histogram() const
{
    std::unique_ptr<RawFeatureHistogramImpl<T>> hist(new RawFeatureHistogramImpl<T>);
    hist->compute_and_bucketize(this, 1<<options.bucket_max_bits);
    return std::move(hist);
}

INSTANTITATE_TEMPLATES_FOR_RAW_FEATURE_TYPES(RawFeature)





DynamicRawFeature::DynamicRawFeature(size_t expected_documents_count, RawFeatureType default_type)
{
    this->expected_documents_count = expected_documents_count;
    this->feature = std::unique_ptr<AbstractRawFeature>(create_raw_feature_for_type(default_type));
    this->type = default_type;
}

void DynamicRawFeature::consume_cell(const char * value)
{
    try {
        this->feature->consume_cell(value);
    } catch (number_format_error& e) {
        (void)e;
        this->upgrade_feature_type(value);
        this->feature->consume_cell(value);
    }
}

void DynamicRawFeature::consume_cell(const char * value, DOC_ID doc_id)
{
    this->feature->set_next_docid(doc_id);
    this->consume_cell(value);
}

void DynamicRawFeature::finalize(DOC_ID total_count)
{
    this->feature->set_next_docid(total_count);
}

AbstractRawFeature * DynamicRawFeature::create_raw_feature_for_type(RawFeatureType type)
{
    switch (type) {
        case RawFeatureType::UINT8:
            return new RawFeature<uint8_t>(this->expected_documents_count);
        case RawFeatureType::INT8:
            return new RawFeature<int8_t>(this->expected_documents_count);
        case RawFeatureType::UINT16:
            return new RawFeature<uint16_t>(this->expected_documents_count);
        case RawFeatureType::INT16:
            return new RawFeature<int16_t>(this->expected_documents_count);
        case RawFeatureType::UINT32:
            return new RawFeature<uint32_t>(this->expected_documents_count);
        case RawFeatureType::INT32:
            return new RawFeature<int32_t>(this->expected_documents_count);
        case RawFeatureType ::FLOAT:
            return new RawFeature<float_t>(this->expected_documents_count);
    }
    return NULL;
}

void DynamicRawFeature::upgrade_feature_type(const char * value)
{
    // This ascertains that the value is a number
    parse_string<float>(value);
    
    // Find a suitable type for the feature
    int current_type_level = static_cast<int>(this->type);
    int potential_type = current_type_level + 1;
    while (potential_type <= static_cast<int>(MAX_RAW_FEATURE_TYPE) ) {
        try {
            std::unique_ptr<ColumnConsumer> test_type(this->create_raw_feature_for_type(static_cast<RawFeatureType>(potential_type)));
            test_type->consume_cell(value);
            test_type->consume_cell(this->feature->get_min_str().c_str());
            test_type->consume_cell(this->feature->get_max_str().c_str());
            break;
        } catch (number_format_error& e) {
            (void)e;
            potential_type++;
        } 
    }
    if (potential_type > static_cast<int>(MAX_RAW_FEATURE_TYPE)) {
        throw std::runtime_error(std::string("Cannot parse value in tsv file as number: ") + value);
    }
    RawFeatureType new_feature_type = static_cast<RawFeatureType>(potential_type);
    
    // Convert the feature
    std::unique_ptr<AbstractRawFeature> new_feature(this->create_raw_feature_for_type(new_feature_type));
    new_feature->import_data(this->feature->export_data().get());
    this->feature = std::move(new_feature);
}

void DynamicRawFeature::set_name(const char * name)
{
    this->name = name;
}

const std::string * DynamicRawFeature::get_name()
{
    return & this->name;
}

std::unique_ptr<RawFeatureHistogram> DynamicRawFeature::to_histogram()
{
    return this->feature->to_histogram();
}


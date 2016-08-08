#ifndef __tealtree__raw_feature__
#define __tealtree__raw_feature__

#include <memory>
#include <stdio.h>
#include <string>
#include <vector>

#include "column_consumer.h"
#include "types.h"


class RawFeatureHistogram;

class AbstractRawFeature : public ColumnConsumer
{
public:
    virtual ~AbstractRawFeature() {};
    virtual void consume_cell(const char * str_value) = 0;
    virtual void set_next_docid(DOC_ID doc_id) = 0;
    virtual std::string get_min_str() const = 0;
    virtual std::string get_max_str() const = 0;
    virtual std::unique_ptr<std::vector<std::string>> export_data() const = 0;
    virtual void import_data(const std::vector<std::string> * data) = 0;
    virtual std::unique_ptr<RawFeatureHistogram> to_histogram() const = 0;
};

template <typename T>
class RawFeature : public AbstractRawFeature
{
private:
    std::vector<T> data;
    T min_element, max_element;
public:
    RawFeature(size_t expected_documents_count = 0);
    virtual ~RawFeature();
    virtual void consume_cell(const char * str_value);
    virtual void set_next_docid(DOC_ID doc_id);
    virtual std::string get_min_str() const;
    virtual std::string get_max_str() const;
    virtual std::unique_ptr<std::vector<std::string>> export_data() const;
    virtual void import_data(const std::vector<std::string> * data);
    const std::vector<T> * get_data() const;
    virtual std::unique_ptr<RawFeatureHistogram> to_histogram() const;
};

class DynamicRawFeature : public ColumnConsumer
{
private:
    size_t expected_documents_count;
    std::unique_ptr<AbstractRawFeature> feature;
    RawFeatureType type;
    std::string name;
public:
    DynamicRawFeature(size_t expected_documents_count = 0, RawFeatureType default_type=RawFeatureType::UINT8);
    virtual ~DynamicRawFeature() {};
    virtual void consume_cell(const char * value);
    virtual void consume_cell(const char * value, DOC_ID doc_id);
    virtual void finalize(DOC_ID total_count);
    virtual void set_name(const char * name);
    virtual const std::string * get_name();
    std::unique_ptr<RawFeatureHistogram> to_histogram();
private:
    AbstractRawFeature * create_raw_feature_for_type(RawFeatureType type);
    void upgrade_feature_type(const char * value);
};


#endif /* defined(__tealtree__raw_feature__) */

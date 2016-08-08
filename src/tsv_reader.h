#ifndef __tealtree__tsv_reader__
#define __tealtree__tsv_reader__

#include "line_reader.h"
#include "raw_feature.h"

#include <random>
#include <vector>

class TabIterator
{
private:
    std::string * s;
    size_t len, ptr;
    char separator;
public:
    TabIterator(std::string * s, char separator);
    inline const char * next();
};

class DataFileReader 
{
protected:
    std::unique_ptr<LineReader> line_reader;
    char separator;
    std::shared_ptr<ColumnConsumerProvider> provider;
    ColumnConsumer * query_consumer;
    ColumnConsumer *label_consumer;
    std::vector<ColumnConsumer *> features;
    float_t input_sample_rate;
    std::mt19937 * mt; 
    std::unique_ptr<std::uniform_real_distribution<float_t>> distribution;
    bool group_by_query;
    std::string last_query_id;
    bool currently_sampling;
    DOC_ID n_docs;
public:
    DataFileReader(std::unique_ptr<LineReader> line_reader, char separator, std::shared_ptr<ColumnConsumerProvider>  ccp);
    virtual ~DataFileReader();
    virtual void read() = 0;
    void set_sample_rate(float_t sample_rate, std::mt19937 * mt, bool group_by_query);
protected:
    bool is_sampled(const std::string & query_id);
};

class TsvReader : public DataFileReader
{
private:
    std::string query_column, label_column;
    std::vector<ColumnConsumer*> consumers;
    size_t query_column_index;
public:
    TsvReader(std::unique_ptr<LineReader> line_reader, char separator, std::shared_ptr<ColumnConsumerProvider>  ccp, std::string query_column, std::string label_column) :
        DataFileReader(std::move(line_reader), separator, ccp),
        query_column(query_column), 
        label_column(label_column) 
    {}
    virtual void read();
private:
    void read_header();
    void read_body();
    void read_row(std::string * row);
    inline std::string get_query_id(const std::string & row);
};

class SvmReader : public DataFileReader
{
private:
    std::string query_prefix;
    bool features_dynamic = true;
public:
    SvmReader(std::unique_ptr<LineReader> line_reader, std::shared_ptr<ColumnConsumerProvider>  ccp, std::string query_prefix) :
        DataFileReader(std::move(line_reader), ' ', ccp),
        query_prefix(query_prefix) 
    {}
    void set_feature_names(std::unique_ptr<std::vector<std::string>> feature_names);
    virtual void read();
private:
    void read_row(std::string * row, size_t line_number);
    inline std::string get_query_id(const std::string & row);

};



#endif /* defined(__tealtree__tsv_reader__) */

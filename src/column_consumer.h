#ifndef jadetree_column_consumer_h
#define jadetree_column_consumer_h

#include <string.h>

#include "types.h"

class ColumnConsumer
{
public:
    virtual ~ColumnConsumer() {};
    virtual void consume_cell(const char * value) = 0;
    virtual void consume_cell(const char * value, DOC_ID doc_id)
    {
        throw std::runtime_error("Not implemented.");
    }
    virtual void finalize(DOC_ID total_count) {};
    virtual void set_name(const char * name) 
    {
        throw std::runtime_error("Not implemented.");
    }
    virtual const std::string * get_name()
    {
        return nullptr;
    }
};

class ColumnIgnorer : public ColumnConsumer
{
public:
    ColumnIgnorer() {};
    virtual ~ColumnIgnorer() {};
    virtual void consume_cell(const char * value) {};
};

class QueryColumnConsumer : public ColumnConsumer
{
private:
    // This contains doc ids of the first element of every query
    //The number of elements in this vector is 1 + (the number of queries) for the convenience of iteration.
    std::vector<DOC_ID> query_limits;
    DOC_ID current_doc_id = 0;
    std::string last_query_id;
public:
    virtual void consume_cell(const char * value)
    {
        if (strcmp(this->last_query_id.c_str(), value) == 0) {
            // do nothing
        }
        else {
            this->last_query_id = value;
            this->query_limits.push_back(this->current_doc_id);
        }
        this->current_doc_id++;
    }
    virtual void finalize(DOC_ID total_count) 
    {
        this->query_limits.push_back(this->current_doc_id);
    }
    std::vector<DOC_ID> * get_query_limits()
    {
        return &this->query_limits;
    }
}; 

class ColumnConsumerProvider
{
public:
    virtual ~ColumnConsumerProvider() {}
    virtual ColumnConsumer* create_label_consumer() = 0;
    virtual ColumnConsumer* create_query_consumer() = 0;
    virtual ColumnConsumer* create_feature_consumer() = 0;
    virtual void on_end_of_row(DOC_ID doc_id) {}
    virtual void on_end_of_file(DOC_ID n_docs) {}
};


#endif

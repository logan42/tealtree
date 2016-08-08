#include "tsv_reader.h"

#include "options.h"
#include "raw_feature.h"

#include <cmath>

TabIterator::TabIterator(std::string * s, char separator)
{
    this->s = s;
    this->len = s->length();
    this->ptr = 0;
    this->separator = separator;
}

inline const char * TabIterator::next()
{
    std::string & ss = *s;
    if (this->ptr >= this->len) {
        return NULL;
    }
    size_t prev_ptr = this->ptr;
    do {
        this->ptr++;
    } while ((this->ptr < this->len) && ( ss[this->ptr] != this->separator));
    if (this->ptr < this->len) {
        ss[this->ptr] = (char)0;
        this->ptr++;
    }
    return ss.c_str() + prev_ptr;
}

DataFileReader::DataFileReader(std::unique_ptr<LineReader> line_reader, char separator, std::shared_ptr<ColumnConsumerProvider> ccp)
{
    this->line_reader = std::move(line_reader);
    this->separator = separator;
    this->provider = ccp;
    this->input_sample_rate = 1;
    this->n_docs = 0;
}

DataFileReader::~DataFileReader()
{
}

void DataFileReader::set_sample_rate(float_t sample_rate, std::mt19937 * mt, bool group_by_query)
{
    this->input_sample_rate = sample_rate;
    this->mt = mt;
    this->distribution = std::unique_ptr<std::uniform_real_distribution<float_t>>(new std::uniform_real_distribution<float_t>(0, 1));
    this->group_by_query = group_by_query;
}

bool DataFileReader::is_sampled(const std::string & query_id)
{
    if (this->input_sample_rate == 1) {
        return true;
    }
    if (this->group_by_query) {
        if (query_id == this->last_query_id) {
            return this->currently_sampling;
        }
        else {
            this->last_query_id = query_id;
        }
    }
    this->currently_sampling = this->input_sample_rate > (*this->distribution)(*this->mt);
    return this->currently_sampling;
}


void TsvReader::read()
{
    this->read_header();
    this->read_body();
}

void TsvReader::read_header()
{
    std::unique_ptr<std::string> header = line_reader->next_line();
    if (header == NULL) {
        throw std::runtime_error(std::string("Cannot read header line in TSV stream."));
    }
    TabIterator header_ti(header.get(), this->separator);
    const char * token;
    
    size_t column_index = 0;
    while ( (token = header_ti.next()) != NULL ) {
        if (strcmp(token, this->label_column.c_str()) == 0) {
            if (this->label_consumer != nullptr) {
                throw std::runtime_error("Label consumer is already created.");
            }
            ColumnConsumer *label_column = this->provider->create_label_consumer();
            this->consumers.push_back(label_column);
            this->label_consumer = label_column;
        } else if (strcmp(token, this->query_column.c_str()) == 0) {
            if (this->query_consumer != nullptr) {
                throw std::runtime_error("Query consumer is already created.");
            }
            this->query_column_index = this->consumers.size();
            ColumnConsumer *query_column = this->provider->create_query_consumer();
            this->consumers.push_back(query_column);
            this->query_consumer = query_column;
        } else {
            // Assuming that this is a feature column
            ColumnConsumer * drf = this->provider->create_feature_consumer();
            drf->set_name(token);
            this->consumers.push_back(drf);
            this->features.push_back(drf);
        }
        column_index++;
    }
    if (this->label_consumer == NULL) {
        throw std::runtime_error(std::string("Could not find label column in TSV file header: ") + this->label_column);
    }
}

void TsvReader::read_body()
{
    std::unique_ptr<std::string> row;
    while ( (row = line_reader->next_line()).get() != NULL ) {
        if (this->is_sampled(this->get_query_id(*row))) {
            this->read_row(row.get());
        }
    }
    for (size_t i = 0; i < this->consumers.size(); i++) {
        consumers[i]->finalize(this->n_docs);
    }
    this->provider->on_end_of_file(this->n_docs);
}

void TsvReader::read_row(std::string * row)
{
    TabIterator ti(row, this->separator);
    const char * token;
    size_t column_index = 0;
    while ( (token = ti.next()) != NULL ) {
        if (column_index >= this->consumers.size()) {
            throw std::runtime_error(std::string("TSV file contains a row with more columns than header"));
        }
        this->consumers[column_index]->consume_cell(token);
        column_index++;
    }
    if (column_index > 0) {
        if (column_index < this->consumers.size()) {
            throw std::runtime_error(std::string("TSV file contains a row with less columns than header"));
        }
        this->provider->on_end_of_row(this->n_docs);
        this->n_docs++;
    }
}

inline std::string TsvReader::get_query_id(const std::string & row)
{
    if (!this->group_by_query) {
        return "";
    }
    if (this->query_consumer == nullptr) {
        throw std::runtime_error("Query column not found.");
    }
    if (row.find(this->separator) == std::string::npos) {
        // Assume this is an empty line
        return "";
    }
    size_t index = 0;
    for (size_t i = 0; i < this->query_column_index; i++) {
        index = row.find(this->separator, index) + 1;
    }
    if (index == std::string::npos) {
        throw std::runtime_error("Query cell not found in a row.");
    }
    size_t index2 = row.find(this->separator, index);
    if (index2 == std::string::npos) {
        index2 = row.size();
    }
    return row.substr(index, index2 - index);
}



void SvmReader::set_feature_names(std::unique_ptr<std::vector<std::string>> feature_names)
{
    this->features_dynamic = false;
    size_t n = feature_names->size();
    this->features.resize(n);
    for (size_t i = 0; i < n; i++) {
        this->features[i] = this->provider->create_feature_consumer();
        this->features[i]->set_name((*feature_names)[i].c_str());
    }
}

void SvmReader::read()
{
    this->label_consumer = this->provider->create_label_consumer();
    this->query_consumer = this->provider->create_query_consumer();
    this->n_docs = 0;

    std::unique_ptr<std::string> row;
    size_t line_number = 0;
    while ((row = line_reader->next_line()).get() != NULL) {
        if (this->is_sampled(this->get_query_id(*row))) {
            this->read_row(row.get(), line_number);
        }
                line_number++;
            }
    for (size_t i = 0; i < this->features.size(); i++) {
        this->features[i]->finalize(this->n_docs);
    }
    if (this->query_consumer != nullptr) {
        this->query_consumer->finalize(this->n_docs);
    }
    this->provider->on_end_of_file(this->n_docs);
}

void SvmReader::read_row(std::string * row, size_t line_number)
{
    TabIterator ti(row, this->separator);
    const char * token;
    bool label_found = false;
    while ((token = ti.next()) != NULL) {
        if (strlen(token) == 0) {
            continue;
        }
        if (token[0] == '#') {
            return;
        }
        this->label_consumer->consume_cell(token);
        label_found = true;
        break;
    }
    if (!label_found) {
        // Well, I guess that's an empty string, we can ignore it
        return;
    }

    bool query_found = false;
    while ((token = ti.next()) != NULL) {
        if (strlen(token) == 0) {
            continue;
        }
        if (token[0] == '#') {
            break;
        }

        char * token2 = const_cast<char*>(token);
        char * delimiter = strchr(token2, ':');
        if (delimiter == nullptr) {
            throw std::runtime_error(std::string("Cannot parse invalid Svm token in line ") + std::to_string(line_number));
        }
        delimiter[0] = 0;

        char * prefix = token2;
        char * value = delimiter + 1;
        size_t feature_index;
        try {
            feature_index = parse_string<size_t>(prefix);
        }
        catch (number_format_error& e) {
            (void)e;
            if ((this->query_prefix.size() > 0) && (strcmp(this->query_prefix.c_str(), prefix) == 0)) {
                this->query_consumer->consume_cell(value);
                query_found = true;
                continue;
            }
            else {
                throw std::runtime_error(std::string("Cannot parse: invalid prefix '") + std::string(prefix) + std::string(" in line ") + std::to_string(line_number));
            }
        }

        if (feature_index >= this->features.size()) {
            if (!this->features_dynamic) {
                throw std::runtime_error("Error parsing Svm: feature id is too large in line " + std::to_string(line_number));
            }
            //this->features.resize(feature_index + 1);
            while (this->features.size() <= feature_index) {
                ColumnConsumer * drf = this->provider->create_feature_consumer();
                std::string feature_name = std::string("Feature") + std::to_string(this->features.size());
                drf->set_name(feature_name.c_str());
                this->features.push_back(drf);
            }
        }
        this->features[feature_index]->consume_cell(value, this->n_docs);
    }
        if (!query_found && (this->query_prefix.size() > 0)) {
            throw std::runtime_error("Error parsing Svm file format: query_id not found in line " + std::to_string(line_number));
        }
        this->provider->on_end_of_row(this->n_docs);
        this->n_docs++;
}

inline std::string SvmReader::get_query_id(const std::string & row)
{
    if (!this->group_by_query) {
        return "";
    }
    std::string substr = " " + this->query_prefix + ":";
    size_t index = row.find(substr);
    if (index == std::string::npos) {
        throw std::runtime_error("query_prefix not found.");
    }
    size_t index2 = row.find(" ", index + 1);
    if (index2 == std::string::npos) {
        index2 = row.size();
    }
    index += substr.size();
    return row.substr(index, index2-index);
}


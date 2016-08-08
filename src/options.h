#ifndef __tealtree__options__
#define __tealtree__options__

#include <math.h>
#include <stdio.h>
#include <string>

#include "types.h"

struct Options
{
    uint32_t logging_severity;
    std::string action;
    std::string input_pipe;
    std::string input_file;
    bool input_stdin;
    std::string input_format;
    std::string feature_names_file;
    std::string output_tree;
    char tsv_separator;
    std::string tsv_label;
    std::string tsv_query;
    std::string svm_query;
    uint32_t default_raw_feature_type;
    uint32_t random_seed;
    float_t input_sample_rate;
    float_t base_score;
    uint32_t expected_documents_count;
    uint32_t bucket_max_bits;
    float_t sparsity_threshold;
    float_t initial_tail_size;
    std::string sparse_feature_version;
     uint32_t n_threads;
    std::string cost_function;
    std::string step;
    bool exponentiate_label;
    uint32_t n_leaves;
    uint32_t max_depth;
    uint32_t n_trees;
    DOC_ID min_node_docs;
    float_t min_node_hessian;
    float_t step_alpha;
    bool quadratic_spread;
    float_t regularization_lambda;
    bool tree_debug_info;

    // Evaluation options:
    std::string input_tree;
    std::string metric;
    std::string output_epochs;
    std::string output_predictions;
};

extern Options options;

void parse_options(int argc, const char * argv[]);

void validate_options();

#endif /* defined(__tealtree__options__) */

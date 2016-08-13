#ifndef __tealtree__options__
#define __tealtree__options__

#include <math.h>
#include <stdio.h>
#include <string>

#include "enum_factory.h"
#include "types.h"

#define InputFormatDefinition(T, XX) \
XX(T, TSV , =1) \
XX(T, SVM, =2) \

// enum class InputFormat { ...
DECLARE_ENUM(InputFormat, InputFormatDefinition)


#define SparseFeatureVersionDefinition(T, XX) \
XX(T, AUTO, =0) \
XX(T, V1, =1) \
XX(T, V2, =2) \

// enum class SparseFeatureVersion{ ...
DECLARE_ENUM(SparseFeatureVersion, SparseFeatureVersionDefinition)

#define StepDefinition(T, XX) \
XX(T, gradient, =0) \
XX(T, newton, =1) \

// enum class Step{ ...
DECLARE_ENUM(Step, StepDefinition)

#define SpreadDefinition(T, XX) \
XX(T, linear, =0) \
XX(T, quadratic, =1) \

// enum class Spread{ ...
DECLARE_ENUM(Spread, SpreadDefinition)


struct Options
{
    uint32_t logging_severity;
    std::string action;
    std::string input_pipe;
    std::string input_file;
    InputFormat input_format;
    std::string feature_names_file;
    std::string output_tree;
    char tsv_separator;
    std::string tsv_label;
    std::string tsv_query;
    std::string svm_query;
    RawFeatureType default_raw_feature_type;
    uint32_t random_seed;
    float_t input_sample_rate;
    float_t base_score;
    uint32_t expected_documents_count;
    uint32_t bucket_max_bits;
    float_t sparsity_threshold;
    float_t initial_tail_size;
    SparseFeatureVersion sparse_feature_version;
     uint32_t n_threads;
    std::string cost_function;
    Step step;
    bool exponentiate_label;
    uint32_t n_leaves;
    uint32_t max_depth;
    uint32_t n_trees;
    DOC_ID min_node_docs;
    float_t min_node_hessian;
    float_t step_alpha;
    Spread spread;
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

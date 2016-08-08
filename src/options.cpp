#include "options.h"

#include "util.h"

#include <algorithm>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

Options options;

void parse_options(int argc, const char * argv[])
{
    po::options_description desc("Options");
    desc.add_options()
        ("help", "Print help messages")
        ("logging_severity", po::value<uint32_t>(&options.logging_severity)->default_value((uint32_t)2), "Minimum severity of logging messages. 0=TRACE, 1=DEBUG, 2=INFO, etc.")
        ("action", po::value<std::string>(&options.action), "Action to perform. Can be either 'train' or 'evaluate'.")
        ("input_pipe", po::value<std::string>(&options.input_pipe), "Command that produces pipe to read the input data from. For reading from plain tsv file use 'cat <filename>'.")
        ("input_file", po::value<std::string>(&options.input_file), "Input file to read training data from.")
        ("input_stdin", po::value<bool>(&options.input_stdin), "Read training data from stdin.")
        ("input_format", po::value<std::string>(&options.input_format), "Input file format. Choices=tsv,svm.")
        ("feature_names_file", po::value<std::string>(&options.feature_names_file), "File containing feature names, optional.")
        ("output_tree", po::value<std::string>(&options.output_tree), "Output file containing the trained tree ensemble.")
        ("tsv_separator", po::value<char>(&options.tsv_separator)->default_value('\t'), "Separator of input TSV file.")
        ("tsv_label", po::value<std::string>(&options.tsv_label)->default_value("Label"), "The name of the 'Label' column in the input TSV file.")
        ("tsv_query", po::value<std::string>(&options.tsv_query)->default_value("Query"), "The name of the 'Query' column in the input TSV file.")
        ("svm_query", po::value<std::string>(&options.svm_query)->default_value(""), "The name of thequery_id prefix  in the input SVM file.")
        ("default_raw_feature_type", po::value<uint32_t>(&options.default_raw_feature_type)->default_value(1), "When parsing a TSV file, Defines the default type of the feature values to expect. For more details, see src\\types.h, enum RawFeatureType.")
        ("random_seed", po::value<uint32_t>(&options.random_seed)->default_value(1), "Initial seed for random numbers generator. Set to 0 for randomized seed.")
        ("input_sample_rate", po::value<float_t>(&options.input_sample_rate)->default_value(1.0), "Subsample only a given fraction of input documents to train. Useful for testing.")
        ("base_score", po::value<float_t>(&options.base_score)->default_value((float_t)0.0), "Base score of the model.")
        ("expected_documents_count", po::value<uint32_t>(&options.expected_documents_count)->default_value(10), "Expected number of documents to preinitialize structures. Setting this value is not required, but setting it to the right count might improve performance.")
        ("bucket_max_bits", po::value<uint32_t>(&options.bucket_max_bits)->default_value(12), "Maximum number of bits to bucketize features. Any feature will be bucketized into at most 2^bucket_max_bits values. Possible values: 1..16.")
        ("sparsity_threshold", po::value<float_t>(&options.sparsity_threshold)->default_value((float_t)0.1), "Feature sparsity threshold. Features that are sparser than this will be encoded in sparse format.")
        ("initial_tail_size", po::value<float_t>(&options.initial_tail_size)->default_value((float_t)0.03), "For fast sparse features defines the initial tail size as a percentage of the offsets buffer.")
        //("disable_fast_sparse_features", po::value<bool>(&options.disable_fast_sparse_features)->default_value(false), "Disable fast sparse feature implementation and use the plain old sparse feature implementation instead.")
        ("sparse_feature_version", po::value<std::string>(&options.sparse_feature_version)->default_value("auto"), "Defines which implementation of the sparse features to use.")
        ("n_threads", po::value<uint32_t>(&options.n_threads)->default_value(0), "Number of threads.")
        ("cost_function", po::value<std::string>(&options.cost_function), "Cost function and objective to solve. Possible values: regression, binary_classification, lambda_rank.")
        ("step", po::value<std::string>(&options.step)->default_value("gradient"), "Step of gradient descent. Possible values: gradient, newton.")
        ("exponentiate_label", po::value<bool>(&options.exponentiate_label), "Performs label = 2^label - 1 transformation. Often used for LambdaRank.")
        ("n_leaves", po::value<uint32_t>(&options.n_leaves)->default_value(100), "Number of leaves per tree.")
        ("max_depth", po::value<uint32_t>(&options.max_depth)->default_value(0), "Maximum depth of a tree ijn the ensemble. Set to 0 to disable.")
        ("n_trees", po::value<uint32_t>(&options.n_trees)->default_value(10), "Number of trees in ensemble.")
        ("min_node_docs", po::value<DOC_ID>(&options.min_node_docs)->default_value(1), "Minimum number of documents in a node.")
        ("min_node_hessian", po::value<float_t>(&options.min_node_hessian)->default_value((float_t)1), "Minimum value of Hessian in a node. This is similar to --min_node_docs for Newton step.")
        ("learning_rate", po::value<float_t>(&options.step_alpha)->default_value((float_t)0.1), "Learning rate, defines the relative step size.")
        ("quadratic_spread", po::value<bool>(&options.quadratic_spread)->default_value(false), "Defines the formula for computing spread value of a split. If false, then absolute value of a difference between average gradients is used. If true, then difference of variances is used.")
        ("regularization_lambda", po::value<float_t>(&options.regularization_lambda)->default_value((float_t)1), "Regularization parameter for quadratic spread.")
        ("tree_debug_info", po::value<bool>(&options.tree_debug_info)->default_value(false), "Whether to store debug information to the output ensemble.")

        ("input_tree", po::value<std::string>(&options.input_tree), "For evaluation: input file containing a trained ensemble.")
        ("metric", po::value<std::string>(&options.metric), "For evaluation: Metric name to compute, if different from the default.")
        ("output_epochs", po::value<std::string>(&options.output_epochs), "For evaluation: optional output file to save the metric value for every epoch to.")
        ("output_predictions", po::value<std::string>(&options.output_predictions), "For evaluation: optional output file to save predictions to.")
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
}

void validate_options()
{
    if ((options.bucket_max_bits <= 0) || (options.bucket_max_bits > 16)) {
        throw po::validation_error(po::validation_error::invalid_option_value, std::string("Invalid value of bucket_max_bits: ") + std::to_string(options.bucket_max_bits));
    }
    if ((options.step_alpha <= 0) || (options.step_alpha > 1000)) {
        throw po::validation_error(po::validation_error::invalid_option_value, std::string("Invalid value of step_alpha: must be between 0.0 and 1000.0, but found ") + std::to_string(options.step_alpha));
    }
    if ((options.min_node_docs <= 0)) {
        throw po::validation_error(po::validation_error::invalid_option_value, std::string("Invalid value of min_node_docs: ") + std::to_string(options.min_node_docs) + std::string(", must be greater than 0."));
    }

    if (options.min_node_hessian  <= 0) {
        throw po::validation_error(po::validation_error::invalid_option_value, "Invalid value of min_node_hessian: " + std::to_string(options.min_node_hessian) + ", must be greater than 0.");
    }

    size_t inputs = 0;
    if (options.input_file.size() > 0) inputs++;
    if (options.input_pipe.size() > 0) inputs++;
    if (options.input_stdin) inputs++;    
    if (inputs != 1) {
        throw po::validation_error(po::validation_error::invalid_option_value, std::string("Exactly one of (input_file, input_pipe, input_stdin) should be specified as input."));
    }

    if ((strcmp(options.input_format.c_str(), "tsv") != 0) && (strcmp(options.input_format.c_str(), "svm") != 0)) {
        throw po::validation_error(po::validation_error::invalid_option_value, std::string("Option input_format can either be tsv or svm."));
    }
    if ((options.input_sample_rate < 0) || (options.input_sample_rate > 1.0)) {
        throw po::validation_error(po::validation_error::invalid_option_value, std::string("Option input_sample_rate must be between 0.0 and 1.0."));
    }

    //if ((options.cost_function != std::string("regression")) && (options.cost_function != std::string("binary_classification")) && (options.cost_function != std::string("lambda_rank"))) {
    //    throw po::validation_error(po::validation_error::invalid_option_value, std::string("Invalid value of Option cost_function."));
    //}

    if ((options.step!= std::string("gradient")) && (options.step!= std::string("pseudonewton")) && (options.step!= std::string("newton"))) {
        throw po::validation_error(po::validation_error::invalid_option_value, std::string("Invalid value of Option step."));
    }

    if ((options.sparsity_threshold > 1.0) || (options.sparsity_threshold < 0)) {
        throw po::validation_error(po::validation_error::invalid_option_value, "sparsity_threshold must be between 0 and 1.");
    }

    if (options.regularization_lambda < 0){
        throw po::validation_error(po::validation_error::invalid_option_value, "regularization_lambda must be non-negative.");
    }

    if ((options.action != "train") && (options.action != "evaluate")) {
        throw po::validation_error(po::validation_error::invalid_option_value, "Invalid action option value.");
    }

    if (options.initial_tail_size < 0) {
        throw po::validation_error(po::validation_error::invalid_option_value, "Option initial_tail_size must be positive.");
    }

    std::vector<std::string> sparse_feature_version_values = {"v1", "v2", "auto"};
    if (std::find(sparse_feature_version_values.begin(), sparse_feature_version_values.end(), options.sparse_feature_version) == sparse_feature_version_values.end()) {
        throw po::validation_error(po::validation_error::invalid_option_value, "Option --sparse_feature_version must be one of (v1, v2, auto).");
    }

}

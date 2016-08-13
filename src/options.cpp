#include "options.h"

#include "util.h"

#include <algorithm>
#include <boost/program_options.hpp>
#include <tclap/CmdLine.h>

namespace po = boost::program_options;

Options options;

template<typename T>
class NumericConstraint : public TCLAP::Constraint<T>
{
protected:
    bool lt, lte, gt, gte;
    T lower, upper;
public:
    NumericConstraint()
        : lt(false),
        lte(false),
        gt(false),
        gte(false),
        lower(0),
        upper(0)
    {}

    NumericConstraint<T> * set_gt(T lower)
    {
        assert(!gt);
        assert(!gte);
        gt = true;
        this->lower = lower;
        return this;
    }

    NumericConstraint<T> * set_gte(T lower)
    {
        assert(!gt);
        assert(!gte);
        gte = true;
        this->lower = lower;
        return this;
    }

    NumericConstraint<T> * set_lt(T upper)
    {
        assert(!lt);
        assert(!lte);
        lt = true;
        this->upper = upper;
        return this;
    }

    NumericConstraint<T> * set_lte(T upper)
    {
        assert(!lt);
        assert(!lte);
        lte = true;
        this->upper = upper;
        return this;
    }

    virtual std::string description() const
    {
        std::string var_name = "x";
        bool l = lt || lte;
        bool g = gt || gte;
        if (!l && !g) return var_name;
        std::ostringstream oss;
        if (l) {
            if (g) {
                oss << lower;
                oss << (gt ? " <" : "<=");
            }
            oss << var_name;
            oss << (lt ? "<" : "<=");
            oss << upper;
        }
        else {
            // g && !l
            oss << var_name;
            oss << (gt ? ">" : ">=");
            oss << lower;
        }
        return oss.str();
}

    virtual std::string shortID() const
    {
        return this->description();
    }

    virtual bool check(const T& value) const
    {
        bool satisfies = true; 
        if (gt && !(value > lower)) satisfies = false;
        if (gte && !(value >= lower)) satisfies = false;
        if (lt && !(value < upper)) satisfies = false;
        if (lte && !(value <= upper)) satisfies = false;
        return satisfies;
    }
};


void parse_options(int argc, const char * argv[])
{
    typedef TCLAP::ValueArg<std::string> TS;
    typedef TCLAP::ValueArg<size_t> TN;
    typedef TCLAP::ValueArg<float_t> TF;
    typedef TCLAP::ValueArg<char> TC;
    typedef TCLAP::SwitchArg TB;

    TCLAP::CmdLine cmd("TealTree gradient boosting decision tree toolkit", ' ', "0.9");

    TN logging_severity_arg("", "logging_severity", "Minimum severity of logging messages; 0=TRACE, 1=DEBUG, 2=INFO, etc.", false, 2, "size_t", cmd);
    TS action_arg("", "action", "Action to perform", true, "", "string", cmd);
    TS input_pipe_arg("", "input_pipe", "Command that produces the input file. The output of this command will be read as input data.", false, "", "string", cmd);
    TS input_file_arg("", "input_file", "Input file to read data from.", false, "", "string", cmd);
    auto input_format_allowed = get_enum_values<InputFormat>();
    TCLAP::ValuesConstraint<std::string> input_format_con(input_format_allowed);
    TS input_format_arg("", "input_format", "Input file format.", true, "", &input_format_con, cmd);
    TS feature_names_file_arg("", "feature_names_file", "File containing feature names.", false, "", "string", cmd);
    TS output_tree_arg("", "output_tree", "Output file containing the trained tree ensemble.", false, "", "string", cmd);
    TC tsv_separator_arg("", "tsv_separator", "Separator of input TSV file.", false, ',', "character", cmd);
    TS tsv_label_arg("", "tsv_label", "The name of the 'Label' column in the input TSV file.", false, "Label", "string", cmd);
    TS tsv_query_arg("", "tsv_query", "The name of the 'Query' column in the input TSV file.", false, "Query", "string", cmd);
    TS svm_query_arg("", "svm_query", "The name of the query_id prefix  in the input SVM file.", false, "qid", "string", cmd);
    auto default_raw_feature_type_allowed = get_enum_values<RawFeatureType>();
    TCLAP::ValuesConstraint<std::string> default_raw_feature_type_con(default_raw_feature_type_allowed);
    TS default_raw_feature_type_arg("", "default_raw_feature_type", "When parsing a TSV or SVM file, Defines the default type of the feature values to expect.", false, "uint8", &default_raw_feature_type_con, cmd);
    TN random_seed_arg("", "random_seed", "Initial seed for random numbers generator. Set to 0 for randomized seed.", false, 1, "size_t", cmd);
    NumericConstraint<float_t> input_sample_rate_con; input_sample_rate_con.set_gte(0)->set_lte(1);
    TF input_sample_rate_arg("", "input_sample_rate", "Subsample only a given fraction of input documents.Useful for testing.", false, 1, &input_sample_rate_con, cmd);
    TF base_score_arg("", "base_score", "Base score of the model.", false, 0, "float", cmd);
    NumericConstraint<size_t> bucket_max_bits_con; bucket_max_bits_con.set_gt(0)->set_lte(16);
    TN bucket_max_bits_arg("", "bucket_max_bits", "Maximum number of bits to represent feature values. Any feature will be bucketized into at most 2^bucket_max_bits values. Possible values: 1..16.", false, 12, &bucket_max_bits_con, cmd);
    NumericConstraint<float_t> sparsity_threshold_con; sparsity_threshold_con.set_gte(0)->set_lte(1);
    TF sparsity_threshold_arg("", "sparsity_threshold", "Feature sparsity threshold. Features that are sparser than this will be encoded in sparse format.", false, (float_t)0.1, &sparsity_threshold_con, cmd);
    NumericConstraint<float_t> initial_tail_size_con; initial_tail_size_con.set_gte(0)->set_lte(1);
    TF initial_tail_size_arg("", "initial_tail_size", "Initial tail size for fast sparse features.", false, (float_t)0.03, &initial_tail_size_con, cmd);
        auto sparse_feature_version_allowed = get_enum_values<SparseFeatureVersion>();
    TCLAP::ValuesConstraint<std::string> sparse_feature_version_con(sparse_feature_version_allowed);
    TS sparse_feature_version_arg("", "sparse_feature_version", "Defines which implementation of the sparse features to use.", false, "auto", &sparse_feature_version_con, cmd);
    TN n_threads_arg("", "n_threads", "Number of threads to use for computation", false, 0, "size_t", cmd);
    TS cost_function_arg("", "cost_function", "Cost function and objective to solve. Possible values: regression, binary_classification, lambda_rank@N.", false, "", "string", cmd);
    auto step_allowed = get_enum_values<Step>();
    TCLAP::ValuesConstraint<std::string> step_con(step_allowed);
    TS step_arg("", "step", "Step of gradient descent. Possible values: gradient, newton.", false, "newton", &step_con, cmd);
    TB exponentiate_label_switch("", "exponentiate_label", "Performs label = 2^label - 1 transformation. Often used for LambdaRank.", cmd, false);
    TN n_leaves_arg("", "n_leaves", "Number of leaves per tree.", false, 0, "size_t", cmd);
    TN max_depth_arg("", "max_depth", "Maximum depth of a tree in the ensemble. Set to 0 to disable.", false, 0, "size_t", cmd);
    TN n_trees_arg("", "n_trees", "Number of trees in the ensemble.", false, 0, "size_t", cmd);
    NumericConstraint<size_t> min_node_docs_con; min_node_docs_con.set_gt(0);
    TN min_node_docs_arg("", "min_node_docs", "Minimum number of documents in a node.", false, 1, &min_node_docs_con, cmd);
    NumericConstraint<float_t> min_node_hessian_con; min_node_hessian_con.set_gt(0);
    TF min_node_hessian_arg("", "min_node_hessian", "Minimum cumulative value of Hessian in a node.", false, (float_t)1.0, &min_node_hessian_con, cmd);
    NumericConstraint<float_t> learning_rate_con; learning_rate_con.set_gt(0);
    TF learning_rate_arg("", "learning_rate", "Learning rate, defines the step size coefficient.", false, (float_t)0.1, &learning_rate_con, cmd);
    auto spread_allowed = get_enum_values<Spread>();
    TCLAP::ValuesConstraint<std::string> spread_con(spread_allowed);
    TS spread_arg("", "spread", "Defines the formula for computing spread value of a split. If linear then absolute value of a difference between average gradients is used. If quadratic then the reduction of variances is used.", false, "quadratic", &spread_con, cmd);
    NumericConstraint<float_t> regularization_lambda_con; regularization_lambda_con.set_gte(0);
    TF regularization_lambda_arg("", "regularization_lambda", "Regularization parameter for quadratic spread.", false, (float_t)1.0, &regularization_lambda_con, cmd);
    TB tree_debug_info_switch("", "tree_debug_info", "Whether to store debug information in the output ensemble.", cmd, false);

    TS input_tree_arg("", "input_tree", "For evaluation: input file containing a trained ensemble.", false, "", "string", cmd);
    TS metric_arg("", "metric", "For evaluation: Metric name to compute, if different from the default.", false, "", "string", cmd);
    TS output_epochs_arg("", "output_epochs", "For evaluation: optional output file to save the metric value for every epoch to.", false, "", "string", cmd);
    TS output_predictions_arg("", "output_predictions", "For evaluation: optional output file to save predictions to.", false, "", "string", cmd);

    cmd.parse(argc, argv);

    options.logging_severity = logging_severity_arg.getValue();
    options.action = action_arg.getValue();
    options.input_pipe = input_pipe_arg.getValue();
    options.input_file = input_file_arg.getValue();
    options.input_format = parse_enum<InputFormat>(input_format_arg.getValue());
    options.feature_names_file = feature_names_file_arg.getValue();
    options.output_tree = output_tree_arg.getValue();
    options.tsv_separator = tsv_separator_arg.getValue();
    options.tsv_label = tsv_label_arg.getValue();
    options.tsv_query = tsv_query_arg.getValue();
    options.svm_query = svm_query_arg.getValue();
    options.default_raw_feature_type = parse_enum<RawFeatureType>(default_raw_feature_type_arg.getValue());
    options.random_seed = random_seed_arg.getValue();
    options.input_sample_rate = input_sample_rate_arg.getValue();
    options.base_score = base_score_arg.getValue();
    options.bucket_max_bits = bucket_max_bits_arg.getValue();
    options.sparsity_threshold = sparsity_threshold_arg.getValue();
    options.initial_tail_size = initial_tail_size_arg.getValue();
    options.sparse_feature_version = parse_enum<SparseFeatureVersion>(sparse_feature_version_arg.getValue());
    options.n_threads = n_threads_arg.getValue();
    options.cost_function = cost_function_arg.getValue();
    options.step = parse_enum<Step>(step_arg.getValue());
    options.exponentiate_label = exponentiate_label_switch.getValue();
    options.n_leaves = n_leaves_arg.getValue();
    options.max_depth = max_depth_arg.getValue();
    options.n_trees = n_trees_arg.getValue();
    options.min_node_docs = min_node_docs_arg.getValue();
    options.min_node_hessian = min_node_hessian_arg.getValue();
    options.step_alpha = learning_rate_arg.getValue();
    options.spread = parse_enum<Spread>(spread_arg.getValue());
    options.regularization_lambda = regularization_lambda_arg.getValue();
    options.tree_debug_info = tree_debug_info_switch.getValue();
    options.input_tree = input_tree_arg.getValue();
    options.metric = metric_arg.getValue();
    options.output_epochs = output_epochs_arg.getValue();
    options.output_predictions = output_predictions_arg.getValue();

    return;

    po::options_description desc("Options");
    desc.add_options()
        ("help", "Print help messages")
        ("logging_severity", po::value<uint32_t>(&options.logging_severity)->default_value((uint32_t)2), "Minimum severity of logging messages. 0=TRACE, 1=DEBUG, 2=INFO, etc.")
        ("action", po::value<std::string>(&options.action), "Action to perform. Can be either 'train' or 'evaluate'.")
        ("input_pipe", po::value<std::string>(&options.input_pipe), "Command that produces pipe to read the input data from. For reading from plain tsv file use 'cat <filename>'.")
        ("input_file", po::value<std::string>(&options.input_file), "Input file to read training data from.")
        //("input_stdin", po::value<bool>(&options.input_stdin), "Read training data from stdin.")
        //("input_format", po::value<std::string>(&options.input_format), "Input file format. Choices=tsv,svm.")
        ("feature_names_file", po::value<std::string>(&options.feature_names_file), "File containing feature names, optional.")
        ("output_tree", po::value<std::string>(&options.output_tree), "Output file containing the trained tree ensemble.")
        ("tsv_separator", po::value<char>(&options.tsv_separator)->default_value('\t'), "Separator of input TSV file.")
        ("tsv_label", po::value<std::string>(&options.tsv_label)->default_value("Label"), "The name of the 'Label' column in the input TSV file.")
        ("tsv_query", po::value<std::string>(&options.tsv_query)->default_value("Query"), "The name of the 'Query' column in the input TSV file.")
        ("svm_query", po::value<std::string>(&options.svm_query)->default_value(""), "The name of thequery_id prefix  in the input SVM file.")
        //("default_raw_feature_type", po::value<uint32_t>(&options.default_raw_feature_type)->default_value(1), "When parsing a TSV file, Defines the default type of the feature values to expect. For more details, see src\\types.h, enum RawFeatureType.")
        ("random_seed", po::value<uint32_t>(&options.random_seed)->default_value(1), "Initial seed for random numbers generator. Set to 0 for randomized seed.")
        ("input_sample_rate", po::value<float_t>(&options.input_sample_rate)->default_value(1.0), "Subsample only a given fraction of input documents to train. Useful for testing.")
        ("base_score", po::value<float_t>(&options.base_score)->default_value((float_t)0.0), "Base score of the model.")
        ("expected_documents_count", po::value<uint32_t>(&options.expected_documents_count)->default_value(10), "Expected number of documents to preinitialize structures. Setting this value is not required, but setting it to the right count might improve performance.")
        ("bucket_max_bits", po::value<uint32_t>(&options.bucket_max_bits)->default_value(12), "Maximum number of bits to bucketize features. Any feature will be bucketized into at most 2^bucket_max_bits values. Possible values: 1..16.")
        ("sparsity_threshold", po::value<float_t>(&options.sparsity_threshold)->default_value((float_t)0.1), "Feature sparsity threshold. Features that are sparser than this will be encoded in sparse format.")
        ("initial_tail_size", po::value<float_t>(&options.initial_tail_size)->default_value((float_t)0.03), "For fast sparse features defines the initial tail size as a percentage of the offsets buffer.")
        // ("sparse_feature_version", po::value<std::string>(&options.sparse_feature_version)->default_value("auto"), "Defines which implementation of the sparse features to use.")
        ("n_threads", po::value<uint32_t>(&options.n_threads)->default_value(0), "Number of threads.")
        ("cost_function", po::value<std::string>(&options.cost_function), "Cost function and objective to solve. Possible values: regression, binary_classification, lambda_rank.")
        //("step", po::value<std::string>(&options.step)->default_value("gradient"), "Step of gradient descent. Possible values: gradient, newton.")
        ("exponentiate_label", po::value<bool>(&options.exponentiate_label), "Performs label = 2^label - 1 transformation. Often used for LambdaRank.")
        ("n_leaves", po::value<uint32_t>(&options.n_leaves)->default_value(100), "Number of leaves per tree.")
        ("max_depth", po::value<uint32_t>(&options.max_depth)->default_value(0), "Maximum depth of a tree ijn the ensemble. Set to 0 to disable.")
        ("n_trees", po::value<uint32_t>(&options.n_trees)->default_value(10), "Number of trees in ensemble.")
        ("min_node_docs", po::value<DOC_ID>(&options.min_node_docs)->default_value(1), "Minimum number of documents in a node.")
        ("min_node_hessian", po::value<float_t>(&options.min_node_hessian)->default_value((float_t)1), "Minimum value of Hessian in a node. This is similar to --min_node_docs for Newton step.")
        ("learning_rate", po::value<float_t>(&options.step_alpha)->default_value((float_t)0.1), "Learning rate, defines the relative step size.")
        //("quadratic_spread", po::value<bool>(&options.quadratic_spread)->default_value(false), "Defines the formula for computing spread value of a split. If false, then absolute value of a difference between average gradients is used. If true, then difference of variances is used.")
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
    //if (options.input_stdin) inputs++;    
    if (inputs != 1) {
        throw po::validation_error(po::validation_error::invalid_option_value, std::string("Exactly one of (input_file, input_pipe, input_stdin) should be specified as input."));
    }

    if ((options.input_sample_rate < 0) || (options.input_sample_rate > 1.0)) {
        throw po::validation_error(po::validation_error::invalid_option_value, std::string("Option input_sample_rate must be between 0.0 and 1.0."));
    }

    //if ((options.cost_function != std::string("regression")) && (options.cost_function != std::string("binary_classification")) && (options.cost_function != std::string("lambda_rank"))) {
    //    throw po::validation_error(po::validation_error::invalid_option_value, std::string("Invalid value of Option cost_function."));
    //}


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


}

DEFINE_ENUM(InputFormat, InputFormatDefinition)
DEFINE_ENUM(SparseFeatureVersion, SparseFeatureVersionDefinition)
DEFINE_ENUM(Step, StepDefinition)
DEFINE_ENUM(Spread, SpreadDefinition)

#include "options.h"

#include "util.h"

#include <algorithm>
#include <tclap/CmdLine.h>

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

inline void flag_assert(bool condition, const std::string & message)
{
    if (condition) {
        return;
    }
    throw TCLAP::ArgException(message);
}


void parse_options(int argc, const char * argv[])
{
    typedef TCLAP::ValueArg<std::string> TS;
    typedef TCLAP::ValueArg<size_t> TN;
    typedef TCLAP::ValueArg<float_t> TF;
    typedef TCLAP::ValueArg<char> TC;
    typedef TCLAP::SwitchArg TB;

    TCLAP::CmdLine cmd("TealTree gradient boosting decision tree toolkit", ' ', "0.9");

    TN logging_severity_arg("", "logging_severity", "Minimum severity of logging messages; 0=TRACE, 1=DEBUG, 2=INFO, etc.", false, 2, "size_t", cmd);
    TB train_switch("", "train", "Train a model.", false);
    TB evaluate_switch("", "evaluate", "Evaluate a model.", false);
    cmd.xorAdd(train_switch, evaluate_switch);
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
    NumericConstraint<size_t> n_trees_con; n_trees_con.set_gt(0);
    TN n_trees_arg("", "n_trees", "Number of trees in the ensemble.", false, 0, &n_trees_con, cmd);
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

    if (train_switch.getValue()) {
        flag_assert(output_tree_arg.isSet(), "--output_tree must be set");
        flag_assert(n_trees_arg.isSet(), "--n_trees must be set");
    }
    if (evaluate_switch.getValue())
    {
        flag_assert(input_tree_arg.isSet(), "--input_tree must be set");
    }

    options.logging_severity = logging_severity_arg.getValue();
    options.train = train_switch.getValue();
    options.evaluate = evaluate_switch.getValue();
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
}


DEFINE_ENUM(InputFormat, InputFormatDefinition)
DEFINE_ENUM(SparseFeatureVersion, SparseFeatureVersionDefinition)
DEFINE_ENUM(Step, StepDefinition)
DEFINE_ENUM(Spread, SpreadDefinition)

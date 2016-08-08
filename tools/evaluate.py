#!/usr/bin/python

import argparse
import csv
import json
import math
import subprocess
import sys

parser = argparse.ArgumentParser(description='Evaluate tree ensemble.')
parser.add_argument("--input_format",
                    help="Input file format. Can be either tsv or svm.", 
                    action="store", required=True, choices=["tsv", "svm"])
parser.add_argument("--input_pipe",
                    help="Input pipe to read data from.", 
                    action="store", required=False)
parser.add_argument("--input_file",
                    help="Input file to read data from.", 
                    action="store", required=False)
parser.add_argument("--input_stdin",
                    help="Take input from stdin to read data from.", 
                    action="store_true")
parser.add_argument("-t", "--input_tree",
                    help="Tree ensemble to evaluate.", 
                    action="store", required=True)
parser.add_argument("--objective",
                    help="Objective for which the tree ensemble was trained.", 
                    action="store", required=True, choices=["regression", "binary_classification", "lambda_rank"])
parser.add_argument("--ndcg_depth",
                    help="How many top results to consider for NDCG metric.", 
                    action="store", type=int, required=False, default=0)
parser.add_argument("-q", "--query_prefix",
                    help="Query prefix in SVM format.", 
                    action="store", required=False)
parser.add_argument("--exponentiate_label",
                    help="Transforms label = 2^label - 1. Usefule for lambda rank.", 
                    action="store_true")
parser.add_argument("--metric_generations_file",
                    help="Output file name, where RMSE values by generation will be stored.", 
                    action="store", required=False)
parser.add_argument("--score_file",
                    help="Output file name, where ensemble scores will be stored.", 
                    action="store", required=False)
args = parser.parse_args()



def to_number(s):
    try:    
        return int(s)
    except ValueError:
        return float(s)

def sigmoid(x):
    exp_x = math.exp(x)
    return exp_x / (1 + exp_x)


class EnsembleEvaluator:
    def __init__(self, tree_file_name, compute_evolutions=False, logistic=False):
        self.root = json.loads(open(tree_file_name, 'r').read())
        self.features = self.root["ensemble"]["features"]
        self.trees = self.root["ensemble"]["trees"]
        self.compute_evolutions = compute_evolutions
        self.logistic = logistic
        for tree in self.trees:
            for node in tree["nodes"]:
                try:
                    node["split"]["threshold"] = to_number(node["split"]["threshold"])
                except KeyError:
                    pass
        self.doc_id = 0
        
    def consume_header(self, header):
        self.feature_to_column = []
        for f in self.features:
            self.feature_to_column.append(header.index(f["name"]))
    
    def consume_svm_row(self, feature_values):
        feature_values = feature_values + ([0.0] * (len(self.features) - len(feature_values)))
        return self.evaluate_ensemble(feature_values)

    def consume_tsv_row(self, row):
        feature_values = [to_number(row[i]) for i in         self.feature_to_column]
        return self.evaluate_ensemble(feature_values)
       
    def evaluate_ensemble(self, feature_values):
        debug = self.doc_id == 29
        scores = [self.evaluate_tree(feature_values, tree, debug=debug) for tree in self.trees]
        for i in xrange(1, len(scores)):
            scores[i] += scores[i-1]
        if not self.compute_evolutions:
            scores = [scores[-1]]
        if self.logistic:
            scores = map(sigmoid, scores)
        self.doc_id += 1
        return scores
        
    def evaluate_tree(self, feature_values, tree, debug=False):
        debug_str = ""
        node_id = 0
        nodes = tree["nodes"]
        while True:
            node = nodes[node_id]
            try:
                result = float(node["value"])
                if math.isnan(result):
                    raise KeyError()
                return result
            except KeyError:
                feature_value = feature_values[node["split"]["feature"]]
                condition = feature_value >= node["split"]["threshold"]
                if condition != node["split"]["inverse"]:
                    node_id = node["right_id"]
                    debug_str += "R"
                else:
                    node_id = node["left_id"]
                    debug_str += "L"
                
                
                
class AveragingMetric:
    def __init__(self, generations_file=None):
        self.numerator = []
        self.denominator = []
        self.gfn= generations_file
    
    def add_entry(self, score):
        if len(self.numerator) == 0:
            self.numerator = [0.0] * len(score)
            self.denominator = [0.0] * len(score)
        for i in xrange(len(score)):
            self.numerator[i] += score[i]
            self.denominator[i] += 1
    
    def get_result(self):
        result = [(self.numerator[i] / self.denominator[i]) for i in xrange(len(self.numerator))]
        return result
    
    def print_result(self):
        result = self.get_result()
        if self.gfn is not None:
            gf = open(self.gfn, "w")
            try:
                gf.write("\n".join(map(str, result)))
            finally:
                gf.close()



class RMSEMetric(AveragingMetric):
    def __init__(self, rmse_generations_file=None):
        AveragingMetric.__init__(self, rmse_generations_file)
    
    def consume_row(self, label, query_id, score):
        self.add_entry([(s - label) ** 2 for s in score])
    
    def get_result(self):
        return map(math.sqrt, AveragingMetric.get_result(self))
    
    def print_result(self):
        result = self.get_result()
        print "RMSE = %f" % result[-1]
        AveragingMetric.print_result(self)


class AccuracyMetric(AveragingMetric):
    def __init__(self, accuracy_generations_file=None):
        AveragingMetric.__init__(self, accuracy_generations_file)
    
    def consume_row(self, label, query_id, score):
        self.add_entry([float((s > 0.5) == (label > 0.5)) for s in score])
    
    def print_result(self):
        result = self.get_result()
        print "Accuracy = %f" % result[-1]
        AveragingMetric.print_result(self)

class GroupByQuery:
    def __init__(self, metric):
        self.metric = metric
        self.last_query_id = ""
        self.labels = []
        self.scores = []
    
    def flush(self):
        if len(self.labels) == 0:
            return

        scores_transposed = [list(i) for i in zip(*self.scores)] 
        #print "qid=%s" % self.last_query_id
        self.metric.consume_query(self.labels, scores_transposed)
        self.scores = []
        self.labels=[]
        
    def consume_row(self, label, query_id, score):
        if query_id != self.last_query_id:
            self.flush()
            self.last_query_id = query_id
        self.labels.append(label)
        self.scores.append(score)

    def print_result(self):
        self.flush()
        self.metric.print_result()

class NDCGMetric(AveragingMetric):
    def __init__(self, ndcg_generations_file, depth):
        AveragingMetric.__init__(self, ndcg_generations_file)

        self.depth = depth

    def get_dcg_coefficient(self, pos):
        return 1.0 / math.log(2.0 + pos, 2.0)

    def calculate_dcg(self, depth, labels, score):
        perm = list(xrange(len(labels)))
        perm = sorted(perm, reverse=True, key=lambda x:score[x])[:depth]
        #print "scores=%s" % (", ".join(map(str, score)))
        #print "perm=%s" % (",".join(map(str, perm)))
        DCG = [labels[i] for i in perm]
        #print "Labels=%s" % (",".join(map(str, DCG)))


        DCG = sum([self.get_dcg_coefficient(i) * x for (i,x) in enumerate(DCG)])
        return DCG

    def consume_query(self, labels, scores):
        this_depth = self.depth
        if this_depth == 0:
            this_depth = len(labels)
        IDCG = sorted(labels, reverse=True)[:this_depth]
        IDCG = sum([self.get_dcg_coefficient(i) * x for (i,x) in enumerate(IDCG)])
        #print "IDCG=%f" % IDCG
        if IDCG == 0:
            # Skip this query
            #print "0 - skip"
            #return
            # ndcgs will be all zeros
            ndcgs= [self.calculate_dcg(this_depth, labels, generation)  for generation in scores]
        else:
            ndcgs= [self.calculate_dcg(this_depth, labels, generation) / IDCG for generation in scores]
        #print "%f" % (ndcgs[-1])
        self.add_entry(ndcgs)
        
    def print_result(self):
        result = self.get_result()
        if self.depth == 0:
            at_str = ""
        else:
            at_str = "@%d" % self.depth
        print "NDCG%s = %f" %(at_str,  result[-1])
        AveragingMetric.print_result(self)
class ScoreSaver:
    def __init__(self, score_file):
        self.score_file = score_file
        if score_file is not None:
            self.f = open(score_file, "w")
            #self.f.write("Label\tScore\n")
        else:
            self.f = None

    def consume_row(self, label, score, query_id=None):
        if self.f is not None:
            #self.f.write("%f\t%f\n" % (label, score[-1]))
              self.f.write("%f\n" % (score[-1]))

    def close(self):
        if self.f is not None:
            self.f.close()

def get_input():
    if args.input_stdin:
        return sys.stdin
    if args.input_pipe is not None:
        proc = subprocess.Popen(args.input_pipe,stdout=subprocess.PIPE, shell=True)
        return proc.stdout
    if args.input_file is not None:
        return open(args.input_file, "r")
    raise Exception("Cannot create input source.")

def preprocess_label(label):
    if args.exponentiate_label:
        return math.pow(2, label) - 1
    return label

def read_data(evaluator, metric, score_saver):
    if args.input_format == "tsv":
        return read_tsv_data(evaluator, metric, score_saver)
    if args.input_format == "svm":
        return read_svm_data(evaluator, metric, score_saver)
    raise Exception("Unknown data format.")

def read_tsv_data(evaluator, metric, score_saver):
    csv_file = csv.reader(get_input(), delimiter='\t')
    header = csv_file.next()
    label_index = header.index("Label")
    evaluator.consume_header(header)
    for row in csv_file:
        label = preprocess_label(float(row[label_index]))
        score = evaluator.consume_tsv_row(row)
        score_saver.consume_row(label, score)
        [metric.consume_row(label, None, score) for metric in metrics]

def read_svm_data(evaluator, metric, score_saver):
    input = get_input()
    for line in input:
        tokens = line.rstrip("\r\n").split()
        label = preprocess_label(float(tokens[0]))
        tokens = [token.split(":") for token in tokens if ":" in token]
        tokens_map = dict(map(tuple, tokens))
        if args.query_prefix is not None:
            query_id = tokens_map[args.query_prefix]
            del tokens_map[args.query_prefix]
        else:
            query_id = None
        tokens_map = dict([(int(k), to_number(v)) for (k,v) in tokens_map.iteritems()])
        feature_values = [0.0] * (max(tokens_map.keys()) + 1)
        for (k,v) in tokens_map.iteritems():
            feature_values[k] = v
        score = evaluator.consume_svm_row(feature_values)
        score_saver.consume_row(label, score)
        [metric.consume_row(label, query_id, score) for metric in metrics]

metrics = []
if args.objective == "regression":
    metrics.append(RMSEMetric(args.metric_generations_file))
if args.objective == "binary_classification":
    #metrics.append(RMSEMetric(args.rmse_generations_file))
    metrics.append(AccuracyMetric(args.metric_generations_file))
if args.objective == "lambda_rank":
    metrics.append(GroupByQuery(NDCGMetric(args.metric_generations_file, args.ndcg_depth)))

score_saver = ScoreSaver(args.score_file)
evaluator = EnsembleEvaluator(
    args.
    input_tree, 
    compute_evolutions=(args.metric_generations_file is not None), 
    logistic=(args.objective == "binary_classification"))    
read_data(evaluator, metrics, score_saver)
[ metric.print_result() for metric in metrics]
score_saver.close()

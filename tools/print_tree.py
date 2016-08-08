#!/usr/bin/python

import argparse
import json

parser = argparse.ArgumentParser(description='Evaluate tree ensemble.')
parser.add_argument("-t", "--input_tree",
                    help="Tree ensemble to evaluate.", 
                    action="store", required=True)
parser.add_argument("--indent",
                    help="Print using this indent character(s).", 
                    action="store", required=False, default="\t")
args = parser.parse_args()

def print_tree_node(features, tree, node_id, indent_level=0):
    node = tree["nodes"][node_id]
    try:
        result = "Value = %f" % node["value"]
    except KeyError:
        threshold = node["split"]["threshold"]  
        if not node["split"]["inverse"]:
            condition_sign = "<"
        else:
            condition_sign = ">="
        feature_name = features[node["split"]["feature"]]["name"]
        left_id = node["left_id"]
        right_id = node["right_id"]
        result = "if ('%s' %s %s) then $%d else $%d" % (feature_name, condition_sign, threshold, left_id, right_id)
        
    if "debug_info" in node:
        debug_info = "# " + ", ".join(["%s:%s" % (key, value) for (key, value) in node["debug_info"].iteritems()])
    else:
        debug_info = ""
    print "%s$%d: %s %s" % (args.indent * indent_level, node_id, result, debug_info)
    try:
        print_tree_node(features, tree, left_id, indent_level + 1)
        print_tree_node(features, tree, right_id, indent_level + 1)
    except NameError:
        pass


def print_tree(features, tree):
    print_tree_node(features, tree, 0)

root = json.loads(open(args.input_tree, 'r').read())
trees = root["ensemble"]["trees"]
features = root["ensemble"]["features"]
for i in xrange(len(trees)):
    print "[Tree#%d]" % i
    print_tree(features, trees[i])

#!/usr/bin/python

import argparse
import os
import platform
import re
import subprocess
import sys
import time

base_dir = os.path.dirname(os.path.realpath(__file__)) + os.sep + ".."
buf_arg = 0
if sys.version_info[0] == 3:
    os.environ['PYTHONUNBUFFERED'] = '1'
    buf_arg = 1
sys.stdout = os.fdopen(sys.stdout.fileno(), 'a+', buf_arg)
sys.stderr = os.fdopen(sys.stderr.fileno(), 'a+', buf_arg)
 

parser = argparse.ArgumentParser(description='Evaluate tree ensemble.')
parser.add_argument("--build",
                    help="Build TealTree before testing", 
                    action="store_true")
args = parser.parse_args()

def build(build_script):
    print "Building..."
    directory = base_dir
    start = time.time()
    p = subprocess.Popen("./" + build_script, cwd=directory, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, close_fds=True)
    count = 0
    for line in p.stdout.readlines():
        line = line.rstrip("\r\n")
        print line
        count += 1
    if count == 0:
        finish = time.time()
        print "Build : OK in %.1f seconds." % (finish - start)
        return True
    else:
        print "Build : Non-empty output, treating as error."
        return False
    

def test(folder, metric_name, expected_values, allowed_error, command="run.sh"):
    ok = False
    message = "Unknown error."
    exception = None
    try:
        directory = os.sep.join([base_dir, "examples", folder])
        command = "./" + command
        values = []
        start = time.time()
        p = subprocess.Popen(command, cwd=directory, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, close_fds=True)
        for line in p.stdout.readlines():
            line = line.rstrip("\r\n")
            m = re.match(metric_name + " = ([0-9.e-]+)", line)
            if m:
                values.append(float(m.group(1)))
        if len(values) == 0:
            message = "No results found."
        elif len(values) < len(expected_values):
            message = "Found fewer results than expected. Found=%d, expected=%d." % (len(values), len(expected_values))
        elif len(values) > len(expected_values):
            message = "Found more results than expected. Found=%d, expected=%d." % (len(values), len(expected_values))
        else:
            finish = time.time()
            ok = True
            message = "OK in %.1f seconds." % (finish - start)
            for i,(v,ev) in enumerate(zip(values, expected_values)):
                if abs(v - ev) >= allowed_error:
                    message = "Wrong result at position %d. Value=%f, expected=%f." % (i, v, ev)
                    ok = False
                    break
    except Exception, e:
        ok = False
        exception = e
        message = "Exception " + str(e)
    
    print "%s : %s" % (folder, message)
    if exception is not None:
        raise Exception(exception)
    return ok

if args.build:
    build_scripts = {"Linux" : "build_linux.sh", "Darwin" : "build_mac.sh"}
    system = platform.system()
    try:
        build_script = build_scripts[system]
    except KeyError:
        print "%s is not supported in this script." % system
        exit()
    ok = build(build_script)
    if not ok:
        exit()
    

reg =  test("regression", "RMSE", [54.8270, 54.8270, 52.7780, 52.7780], 0.001)
bc =   test("binary_classification", "Accuracy", [0.9988, 1.0000], 0.0001)
rank = test("ranker", "NDCG@10", [0.513, 0.549, 0.487], 0.001)
#mslr = test("MSLR", "NDCG", [0.7707, 0.6837], 0.0001, command="run_10percent.sh")

tests = [reg,bc,rank]

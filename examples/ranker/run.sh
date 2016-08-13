#!/bin/bash

BASE=../..
INPUT_FILE=MQ2008.rar

if [ ! -f $INPUT_FILE ]; then
  echo "Downloading LETOR4 dataset from the Internet..."
  curl http://research.microsoft.com/en-us/um/beijing/projects/letor/LETOR4.0/Data/MQ2008.rar > $INPUT_FILE
fi

unrar p -inul  $INPUT_FILE  MQ2008/Fold1/train.txt  \
 | $BASE/bin/tealtree \
 --train \
 --input_format svm \
 --bucket_max_bits 8 \
 --cost_function lambda_rank@10 \
 --step gradient \
 --exponentiate_label \
 --n_leaves 10 \
 --n_trees 40 \
 --learning_rate 0.1 \
 --output_tree forest.json


 EVAL="$BASE/bin/tealtree \
 --logging_severity 5 \
 --evaluate \
 --input_format svm \
 --exponentiate_label \
 --metric ndcg@10 \
 --input_tree forest.json"

 echo "Evaluating on training data:"
 unrar p -inul  $INPUT_FILE  MQ2008/Fold1/train.txt | $EVAL

echo "Evaluating on validation data:"
unrar p -inul  $INPUT_FILE  MQ2008/Fold1/vali.txt  | $EVAL \
 --output_predictions pred_vali.txt \
 --output_epochs epochs_vali.txt

 echo "Evaluating on testing data:"
 unrar p -inul  $INPUT_FILE  MQ2008/Fold1/test.txt  | $EVAL \
 --output_predictions pred_test.txt \
 --output_epochs epochs_test.txt

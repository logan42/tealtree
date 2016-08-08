#!/bin/bash

BASE=../..
INPUT_FILE=MSLR-WEB30K.zip
FEATURE_NAMES_FILE=feature_names.txt

SAMPLE_RATE=${1:-1.0}

if [ ! -f $INPUT_FILE ]; then
  echo "Downloading MSLR dataset from Internet..."
  curl http://research.microsoft.com/en-us/um/beijing/projects/mslr/data/MSLR-WEB30K.zip > $INPUT_FILE
fi

unzip -p $INPUT_FILE Fold1/train.txt \
 | $BASE/bin/tealtree \
 --action=train \
 --input_stdin=true \
 --input_format=svm \
 --svm_query=qid \
 --default_raw_feature_type=3 \
 --input_sample_rate=$SAMPLE_RATE \
 --feature_names_file=$FEATURE_NAMES_FILE \
 --cost_function=lambda_rank \
 --step=newton \
 --quadratic_spread=true \
 --exponentiate_label=true \
 --min_node_docs=1 \
 --n_leaves=150 \
 --n_trees=150 \
 --learning_rate=0.1 \
 --output_tree=forest.json
 

EVAL="$BASE/bin/tealtree \
 --logging_severity=5 \
 --action=evaluate \
 --input_stdin=true \
 --input_format=svm \
 --svm_query=qid \
 --input_sample_rate=$SAMPLE_RATE \
 --exponentiate_label=true \
 --input_tree=forest.json"

 
echo "Evaluating on training data:"
unzip -p $INPUT_FILE Fold1/train.txt  | $EVAL \
 --output_predictions=pred_train.txt \
 --output_epochs=epochs_train.txt

echo "Evaluating on testing data:"
unzip -p $INPUT_FILE Fold1/test.txt  | $EVAL \
 --output_predictions=pred_test.txt \
 --output_epochs=epochs_test.txt



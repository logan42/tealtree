@echo off

set BASE=..\..

%BASE%\bin\tealtree ^
 --train ^
 --input_file agaricus.txt.train ^
 --input_format svm ^
 --feature_names_file feature_names.txt ^
 --cost_function binary_classification ^
 --step newton ^
 --n_leaves 7 ^
 --max_depth 3 ^
 --n_trees 2 ^
 --learning_rate 1.0 ^
 --output_tree forest.json ^
 --tree_debug_info

echo Evaluating on training data:
%BASE%\bin\tealtree ^
 --evaluate ^
 --input_file agaricus.txt.train ^
 --input_format svm ^
 --input_tree forest.json

echo Evaluating on testing data:
%BASE%\bin\tealtree ^
 --evaluate ^
 --input_file agaricus.txt.test ^
 --input_format svm ^
 --input_tree forest.json ^
 --output_predictions pred_test.txt

echo Printing trained ensemble:
python %BASE%\tools\print_tree.py --input_tree forest.json
 


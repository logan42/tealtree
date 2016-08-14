@echo off

set BASE=..\..

%BASE%\bin\tealtree ^
 --train ^
 --input_file machine.txt.train ^
 --input_format svm ^
 --feature_names_file feature_names.txt ^
 --cost_function regression ^
 --regularization_lambda 0 ^
 --n_leaves 7 ^
 --n_trees 2 ^
 --learning_rate 1.0 ^
 --output_tree forest.json ^
 --tree_debug_info

echo Evaluating on training data:
%BASE%\bin\tealtree ^
 --evaluate ^
 --input_file machine.txt.train ^
 --input_format svm ^
 --input_tree forest.json

python %BASE%\tools\evaluate.py ^
 --input_file machine.txt.train ^
 --input_format svm ^
 --input_tree forest.json ^
 --objective regression

echo Evaluating on testing data:
%BASE%\bin\tealtree ^
 --evaluate ^
 --input_file machine.txt.test ^
 --input_format svm ^
 --input_tree forest.json ^
 --output_predictions pred_test.txt

python %BASE%\tools\evaluate.py ^
 --input_file machine.txt.test ^
 --input_format svm ^
 --input_tree forest.json ^
 --objective regression

echo Printing trained ensemble:
python %BASE%\tools\print_tree.py --input_tree forest.json

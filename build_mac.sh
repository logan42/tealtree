#!/bin/bash

mkdir bin 2>/dev/null
rm -rf bin/tealtree* 2>/dev/null

clang++ -Ofast -std=c++14 \
 -DBOOST_LOG_DYN_LINK \
 -DNDEBUG \
 -DGHEAP_CPP11 \
 -Iinclude \
 -I/opt/local/include \
 -L/opt/local/lib \
 -lboost_log-mt \
 -lboost_log_setup-mt \
 -lboost_system-mt \
 -lboost_program_options-mt \
 -lboost_thread-mt \
 -o bin/tealtree \
 src/*.cpp 

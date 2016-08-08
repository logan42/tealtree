#!/bin/bash

mkdir bin 2>/dev/null
rm -rf bin/tealtree* 2>/dev/null

g++ -O3 -std=c++11 -Wall -m64 -msse2 -msse3 \
 -DNDEBUG \
 -I./include \
 -I/usr/local/include \
 -L/usr/local/lib \
 -o bin/tealtree \
 src/*.cpp  \
 -Wl,-Bstatic \
 -lboost_log \
 -lboost_log_setup \
 -lboost_program_options \
 -lboost_system \
 -lboost_thread \
 -Wl,-Bdynamic \
 -lpthread 


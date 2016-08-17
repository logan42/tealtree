#!/bin/bash

mkdir bin 2>/dev/null
rm -rf bin/tealtree* 2>/dev/null

clang++ -Ofast -flto -std=c++14 \
 -DNDEBUG \
 -DGHEAP_CPP11 \
 -Iinclude \
 -o bin/tealtree \
 src/*.cpp 

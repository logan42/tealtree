#!/bin/bash

mkdir bin 2>/dev/null
rm -rf bin/tealtree* 2>/dev/null

g++ -O3 -std=c++11 -Wall -m64 -msse2 -msse3 \
 -DNDEBUG \
 -DGHEAP_CPP11 \
 -I./include \
 -o bin/tealtree \
 src/*.cpp  \
 -lpthread 


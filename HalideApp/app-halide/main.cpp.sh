#!/bin/bash

# Build
g++ -o app-halide \
main.cpp \
`pkg-config --cflags --libs libpng` \
`pkg-config --cflags --libs libjpeg` \
-I ../halide/include -I ../halide/tools \
-L ../halide/bin \
-lHalide -lpthread -ldl -lrt \
-pthread -std=c++11

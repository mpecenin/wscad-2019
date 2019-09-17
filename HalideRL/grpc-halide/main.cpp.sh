#!/bin/bash

# Build
g++ -o grpc-halide \
main.cpp schedule.grpc.pb.cc schedule.pb.cc \
`pkg-config --cflags --libs libpng` \
`pkg-config --cflags --libs libjpeg` \
`pkg-config --cflags --libs protobuf` \
`pkg-config --cflags --libs grpc++` \
`pkg-config --cflags --libs grpc` \
-I ../halide/include -I ../halide/tools \
-L ../halide/bin \
-lHalide -lpthread -ldl -lrt \
-pthread -std=c++11

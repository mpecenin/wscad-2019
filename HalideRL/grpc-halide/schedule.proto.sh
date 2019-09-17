#!/bin/bash

# Server and Client RPC
protoc --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` schedule.proto
protoc --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_python_plugin` schedule.proto
# Message Class
protoc --cpp_out=. schedule.proto
protoc --python_out=. schedule.proto


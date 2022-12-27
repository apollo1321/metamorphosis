#!/usr/bin/env bash

set -e

mkdir -p build-release 
cd build-release
cmake -DCMAKE_BUILD_TYPE=Release -G=Ninja ..
ninja queue_client queue_service

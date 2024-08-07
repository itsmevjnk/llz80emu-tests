#!/bin/bash

set -e # stop upon error

echo "pulling latest commit of submodules"
git submodule update --init --remote --recursive

echo "building tester"
mkdir -p build && cd build
cmake .. && make
cd ..

echo "running tests"
for tc in tests/v1/*.json; do
    ./build/tester/tester "$tc"
done

echo "tests completed"
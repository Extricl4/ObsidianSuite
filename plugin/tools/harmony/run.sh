#!/bin/sh
# Builds and runs the HYDRA bench (plugin/tools/harmony). Needs Signalsmith Stretch + Linear headers:
#   STRETCH=/path/to/signalsmith-stretch LINEAR=/path/to/signalsmith-linear ./run.sh [test]
set -e
cd "$(dirname "$0")"
STRETCH=${STRETCH:-/home/claude/deps/stretch}
LINEAR=${LINEAR:-/home/claude/deps/signalsmith-linear}
g++ -std=c++17 -O3 $CXXFLAGS -march=x86-64-v2 -I../../Source/DSP -I"$STRETCH/include" -I"$LINEAR/include" bench.cpp -o bench
./bench "$@"

#!/usr/bin/env bash
# Builds Terminus on Linux (cmake, a C++17 compiler, libsdl2-dev). AliceVision: put its bin/ and share/ in
# runtime/alicevision (Meshroom 2025.1's Linux build has them under aliceVision/).
set -e
cd "$(dirname "$0")"
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
echo "Built: build/Terminus"

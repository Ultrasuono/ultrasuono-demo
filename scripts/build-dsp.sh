#!/usr/bin/env bash
set -euo pipefail
em++ dsp/AudioProcessor.cpp dsp/bridge.cpp dsp/vendor/Effects/*.cpp dsp/vendor/Filters/*.cpp \
  -Idsp -Idsp/vendor -Idsp/vendor/Utility -O3 -ffast-math -std=c++17 \
  -Wno-ignored-attributes --no-entry -s STANDALONE_WASM=1 -s INITIAL_MEMORY=16777216 \
  -s ALLOW_MEMORY_GROWTH=0 -s EXPORTED_FUNCTIONS='["_ultra_init","_ultra_input","_ultra_output","_ultra_process"]' \
  -o assets/audio/ultrasuono.wasm

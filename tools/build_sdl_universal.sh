#!/usr/bin/env bash
# Build SDL 2.32.10 from the official source as one static library for Apple
# chips and Intel (macOS 11+), so the Mac app needs nothing installed.
set -euo pipefail
cd "$(dirname "$0")/.."
[ -d vendor/SDL2/.git ] || git clone -q --depth 1 --branch release-2.32.10 https://github.com/libsdl-org/SDL.git vendor/SDL2
cmake -S vendor/SDL2 -B vendor/SDL2/build-universal -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF -DSDL_STATIC_PIC=ON \
    -DCMAKE_INSTALL_PREFIX="$PWD/vendor/SDL2-universal" >/dev/null
cmake --build vendor/SDL2/build-universal --parallel 8 >/dev/null
cmake --install vendor/SDL2/build-universal >/dev/null
lipo -info vendor/SDL2-universal/lib/libSDL2.a

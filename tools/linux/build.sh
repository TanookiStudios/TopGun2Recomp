#!/usr/bin/env bash
# Runs INSIDE the build container (tools/make_linux.sh starts it).
# Builds SDL 2.32.10 static, then the game, into build-linux/.
set -euo pipefail
cd /src
if [ ! -f vendor/SDL2-linux/lib/libSDL2.a ]; then
    rm -rf /tmp/sdl && git clone -q --depth 1 --branch release-2.32.10 https://github.com/libsdl-org/SDL.git /tmp/sdl
    cmake -S /tmp/sdl -B /tmp/sdl/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF -DSDL_STATIC_PIC=ON \
        -DCMAKE_INSTALL_PREFIX=/src/vendor/SDL2-linux >/dev/null
    cmake --build /tmp/sdl/build --parallel "$(nproc)" >/dev/null
    cmake --install /tmp/sdl/build >/dev/null
fi
[ -d build-linux ] || cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DSDL2_DIR=/src/vendor/SDL2-linux/lib/cmake/SDL2 \
    -DCMAKE_DISABLE_FIND_PACKAGE_Freetype=ON -DCMAKE_DISABLE_FIND_PACKAGE_PkgConfig=ON \
    -DNESRECOMP_RECOMP_UI=/src/vendor/recomp-ui
cmake --build build-linux --parallel "$(nproc)"

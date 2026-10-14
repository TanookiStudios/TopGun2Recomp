# Build machine for the Linux release: Ubuntu 20.04 (glibc 2.31) so the
# program runs on nearly every Linux desktop from 2020 on. Players need none
# of this; it only builds.
FROM --platform=linux/amd64 ubuntu:20.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl git build-essential ninja-build pkg-config python3-pip \
        libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxinerama-dev libxss-dev \
        libxkbcommon-dev libwayland-dev wayland-protocols libegl1-mesa-dev libgl1-mesa-dev libgles2-mesa-dev \
        libasound2-dev libpulse-dev libdbus-1-dev libudev-dev libdrm-dev libgbm-dev file \
    && rm -rf /var/lib/apt/lists/* \
    && pip3 install --no-cache-dir uv==0.8.22 cmake==3.31.6 \
    && UV_PYTHON_INSTALL_DIR=/opt/python uv python install 3.11 \
    && ln -sf "$(UV_PYTHON_INSTALL_DIR=/opt/python uv python find 3.11)" /usr/local/bin/python3 \
    && ln -sf /usr/local/bin/python3 /usr/local/bin/python3.11

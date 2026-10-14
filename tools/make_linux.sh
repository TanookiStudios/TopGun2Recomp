#!/usr/bin/env bash
# Build the Linux release on this Mac inside an Ubuntu 20.04 container (Colima)
# and pack dist/TopGun2-Linux.tar.gz, copied to ~/Downloads. No ROM inside.
# Needs: brew install colima docker; colima start --vm-type vz --vz-rosetta
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p work/docker-config; [ -f work/docker-config/config.json ] || echo '{}' > work/docker-config/config.json
export DOCKER_CONFIG="$PWD/work/docker-config" DOCKER_HOST="unix://$HOME/.colima/default/docker.sock"
docker image inspect topgun2-linux-build >/dev/null 2>&1 || docker build --platform linux/amd64 -t topgun2-linux-build tools/linux
docker run --rm --platform linux/amd64 -v "$PWD":/src topgun2-linux-build /src/tools/linux/build.sh >/dev/null
OUT=dist/TopGun2-Linux
rm -rf "$OUT" "$OUT.tar.gz"; mkdir -p "$OUT"
cp build-linux/TopGun2 "$OUT/"
cp -R build-linux/assets "$OUT/assets"
cp assets/icon/topgun2-256.png "$OUT/topgun2.png"
cp tools/linux/topgun2.desktop tools/linux/README-Linux.txt tools/linux/install-shortcut.sh "$OUT/"
tar -C dist -czf "$OUT.tar.gz" TopGun2-Linux
# Finished games always go to Downloads (Maddie, 14 Oct 2026).
cp -f "$OUT.tar.gz" "$HOME/Downloads/"
echo "$HOME/Downloads/TopGun2-Linux.tar.gz"

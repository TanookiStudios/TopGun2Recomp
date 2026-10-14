#!/usr/bin/env bash
# Cross-build the Windows version on this Mac and zip it into dist/.
# Needs: brew install mingw-w64. The zip holds no ROM: the launcher asks for one.
set -euo pipefail
cd "$(dirname "$0")/.."
[ -d build-win ] || cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake \
    -DTOPGUN2_RECOMPILER="$PWD/build/nesrecomp-host/NESRecomp" \
    -DNESRECOMP_RECOMP_UI="$PWD/vendor/recomp-ui" >/dev/null
cmake --build build-win --parallel 8 >/dev/null
OUT="dist/TopGun2-Windows"
rm -rf "$OUT" "$OUT.zip"
mkdir -p "$OUT"
cp build-win/TopGun2.exe build-win/SDL2.dll "$OUT/"
cp -R build-win/assets "$OUT/assets"
cp LICENSE THIRD_PARTY_NOTICES.md "$OUT/"
( cd dist && zip -qr TopGun2-Windows.zip TopGun2-Windows )
# Finished games always go to Downloads (Maddie, 14 Oct 2026).
cp -f dist/TopGun2-Windows.zip "$HOME/Downloads/"
echo "$HOME/Downloads/TopGun2-Windows.zip"

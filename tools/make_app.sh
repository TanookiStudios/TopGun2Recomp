#!/usr/bin/env bash
# Build "Top Gun 2.app" for every Mac (Apple chips and Intel, macOS 11+),
# signed with the Developer ID. Copies it to ~/Downloads.
#
#   tools/make_app.sh            build + sign
#   tools/make_app.sh --notarize also notarize with Apple, staple, and zip
#                                (dist/TopGun2-Mac.zip, copied to ~/Downloads)
#
# Needs: vendor/ from setup.sh, the universal SDL2 (tools/build_sdl_universal.sh),
# a "Developer ID Application" certificate, and the notarytool keychain profile
# "notarytool". The app contains no ROM: the launcher asks for one.
set -euo pipefail
cd "$(dirname "$0")/.."
NOTARIZE=0; [ "${1:-}" = "--notarize" ] && NOTARIZE=1
ID="Developer ID Application"

[ -f vendor/SDL2-universal/lib/libSDL2.a ] || tools/build_sdl_universal.sh
[ -d build-mac ] || cmake -S . -B build-mac -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DSDL2_DIR="$PWD/vendor/SDL2-universal/lib/cmake/SDL2" \
    -DCMAKE_DISABLE_FIND_PACKAGE_Freetype=ON -DCMAKE_DISABLE_FIND_PACKAGE_PkgConfig=ON \
    -DNESRECOMP_RECOMP_UI="$PWD/vendor/recomp-ui" >/dev/null
cmake --build build-mac --parallel 8 >/dev/null

APP="dist/Top Gun 2.app"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp build-mac/TopGun2 "$APP/Contents/MacOS/TopGun2"
cp -R build-mac/assets "$APP/Contents/MacOS/assets"
cp assets/icon/AppIcon.icns "$APP/Contents/Resources/AppIcon.icns"
VERSION=$(git describe --tags --abbrev=0 2>/dev/null | sed 's/^v//' || true); VERSION=${VERSION:-0.1.0}
cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>CFBundleName</key><string>Top Gun 2</string>
  <key>CFBundleDisplayName</key><string>Top Gun 2</string>
  <key>CFBundleIdentifier</key><string>com.tanookistudios.topgun2recomp</string>
  <key>CFBundleExecutable</key><string>TopGun2</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleIconFile</key><string>AppIcon</string>
  <key>CFBundleShortVersionString</key><string>$VERSION</string>
  <key>CFBundleVersion</key><string>$VERSION</string>
  <key>LSMinimumSystemVersion</key><string>11.0</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>LSApplicationCategoryType</key><string>public.app-category.arcade-games</string>
</dict></plist>
PLIST
codesign --force --deep --options runtime --timestamp --sign "$ID" "$APP" >/dev/null
codesign --verify --deep --strict "$APP"

if [ "$NOTARIZE" = 1 ]; then
    ZIP=dist/TopGun2-Mac.zip
    rm -f "$ZIP"
    ditto -c -k --keepParent "$APP" "$ZIP"
    xcrun notarytool submit "$ZIP" --keychain-profile notarytool --wait
    xcrun stapler staple "$APP"
    rm -f "$ZIP"
    ditto -c -k --keepParent "$APP" "$ZIP"
    spctl --assess --type execute --verbose "$APP"
    cp -f "$ZIP" "$HOME/Downloads/"
fi

# Earlier builds kept settings inside the app; move them to where this build
# looks (the game also does this itself on first launch).
OLD="$HOME/Downloads/Top Gun 2.app/Contents/MacOS"
NEW="$HOME/Library/Application Support/TopGun2"
mkdir -p "$NEW"
for f in config.ini rom.cfg launcher-window.ini; do
    if [ -e "$OLD/$f" ] && [ ! -e "$NEW/$f" ]; then cp "$OLD/$f" "$NEW/$f"; fi
done
# Finished games always go to Downloads (Maddie, 14 Oct 2026).
rm -rf "$HOME/Downloads/Top Gun 2.app"
cp -R "$APP" "$HOME/Downloads/"
echo "$HOME/Downloads/Top Gun 2.app"

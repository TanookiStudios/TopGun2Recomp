#!/usr/bin/env bash
# Package the windowed Mac build as "Top Gun 2.app" in dist/.
# The app contains no ROM: the launcher asks for your own copy the first time.
#
# shortcut: links Homebrew's SDL2/freetype/harfbuzz in place, so the app runs on
# Macs with those installed (this one). Bundle the dylibs before sharing it.
set -euo pipefail
cd "$(dirname "$0")/.."
cmake --build build --parallel 8 >/dev/null
APP="dist/Top Gun 2.app"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS"
cp build/TopGun2 "$APP/Contents/MacOS/TopGun2"
cp -R build/assets "$APP/Contents/MacOS/assets"
cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>CFBundleName</key><string>Top Gun 2</string>
  <key>CFBundleDisplayName</key><string>Top Gun 2</string>
  <key>CFBundleIdentifier</key><string>com.tanookistudios.topgun2recomp</string>
  <key>CFBundleExecutable</key><string>TopGun2</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>0.1.0</string>
  <key>CFBundleVersion</key><string>1</string>
  <key>LSMinimumSystemVersion</key><string>13.0</string>
  <key>NSHighResolutionCapable</key><true/>
</dict></plist>
PLIST
codesign --force --deep --sign - "$APP" 2>/dev/null
# Finished games always go to Downloads (Maddie, 14 Oct 2026).
# Keep the player's settings, remembered ROM and saves across rebuilds.
DL="$HOME/Downloads/Top Gun 2.app/Contents/MacOS"
KEEP="$(mktemp -d)"
for f in config.ini rom.cfg launcher-window.ini saves; do
    [ -e "$DL/$f" ] && cp -R "$DL/$f" "$KEEP/"
done
rm -rf "$HOME/Downloads/Top Gun 2.app"
cp -R "$APP" "$HOME/Downloads/"
cp -R "$KEEP"/. "$DL/" 2>/dev/null || true
rm -rf "$KEEP"
codesign --force --deep --sign - "$HOME/Downloads/Top Gun 2.app" 2>/dev/null
echo "$HOME/Downloads/Top Gun 2.app"

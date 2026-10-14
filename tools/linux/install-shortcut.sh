#!/bin/sh
# Adds Top Gun 2 to the application menu for this user only.
here="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HOME/.local/share/applications" "$HOME/.local/share/icons/hicolor/256x256/apps"
cp "$here/topgun2.png" "$HOME/.local/share/icons/hicolor/256x256/apps/topgun2.png"
sed "s|^Exec=.*|Exec=\"$here/TopGun2\"|" "$here/topgun2.desktop" > "$HOME/.local/share/applications/topgun2.desktop"
echo "Added Top Gun 2 to your application menu."

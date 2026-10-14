#!/usr/bin/env bash
# Release screenshots, unattended, into docs/screenshots/.
#   gameplay: headless runs of scripted routes (no window), enlarged 3x
#   launcher: the launcher's LNG_SCRIPT mode - its window shows for ~5 seconds
# Fails if any expected picture is missing. Needs rom/topgun2.nes, build/ and
# build-headless/ (the ROM never leaves this machine; only pictures are written).
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=docs/screenshots; TMP=work/shots-release
rm -rf "$TMP"; mkdir -p "$OUT" "$TMP"
H=build-headless/TopGun2; ROM=rom/topgun2.nes
shot() {  # shot <name> <route> <frame>
    "$H" "$ROM" --headless --frames "$3" --input "tests/$2" --screenshot "$TMP/$1.png" >/dev/null 2>&1
    python3 tools/png_scale.py "$TMP/$1.png" "$OUT/$1.png" 3
}
shot gameplay-intro    route_idle.txt       1200
shot gameplay-briefing route_briefing.txt   1700
shot gameplay-flight   route_mission1.txt   4200
shot gameplay-banking  route_mission1.txt   4305

cp tests/screenshot-config.ini "$TMP/config.ini"
S="$PWD/$TMP"
LNG_SCRIPT="wait:40;shot:$S/launcher-main.png;view:game;wait:15;shot:$S/launcher-game.png;view:about;wait:15;shot:$S/launcher-about.png;quit" \
    build/TopGun2 "$ROM" --config "$S/config.ini" >/dev/null 2>&1
for n in launcher-main launcher-game launcher-about; do cp "$TMP/$n.png" "$OUT/$n.png"; done

missing=0
for n in gameplay-intro gameplay-briefing gameplay-flight gameplay-banking launcher-main launcher-game launcher-about; do
    [ -s "$OUT/$n.png" ] || { echo "MISSING: $n" >&2; missing=1; }
done
[ "$missing" = 0 ] || exit 1
ls "$OUT"

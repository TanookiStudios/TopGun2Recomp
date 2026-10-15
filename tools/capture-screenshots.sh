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
shot() {  # shot <name> <route> <frame> [extra options]
    "$H" "$ROM" --headless --frames "$3" --input "tests/$2" --screenshot "$TMP/$1.png" ${4:-} >/dev/null 2>&1
    python3 tools/png_scale.py "$TMP/$1.png" "$OUT/$1.png" 3
}
shot gameplay-intro    route_idle.txt       1200
shot gameplay-briefing route_briefing.txt   1700
shot gameplay-flight   route_mission1.txt   4200
shot gameplay-banking  route_mission1.txt   4305
shot gameplay-deck     route_idle.txt       2796
shot gameplay-mission2 route_mission1.txt   4500 "--start-mission 2 --cheat nohit"
shot gameplay-mission3 route_mission1.txt   4500 "--start-mission 3 --cheat nohit"

# The Esc menu over gameplay, from the real window kept hidden, over its debug port.
cp tests/screenshot-config.ini "$TMP/menu.ini"
printf '[Launcher]\nSkipLauncher = 1\n' >> "$TMP/menu.ini"
build/TopGun2 "$ROM" --hidden --tcp 4399 --config "$PWD/$TMP/menu.ini" --window-input tests/route_mission1.txt >/dev/null 2>&1 &
WIN=$!
for _ in $(seq 1 240); do
    f=$(python3 tools/tcp_cmd.py 4399 '{"cmd":"ping","id":1}' 2>/dev/null | python3 -c "import json,sys; print(json.load(sys.stdin)['frame'])" 2>/dev/null || echo 0)
    [ "$f" -ge 4200 ] && break; sleep 0.5
done
python3 tools/tcp_cmd.py 4399 '{"cmd":"menu","open":true,"id":2}' >/dev/null; sleep 1
python3 tools/tcp_cmd.py 4399 "{\"cmd\":\"screenshot\",\"layer\":\"ui\",\"path\":\"$PWD/$OUT/ingame-menu.png\",\"id\":3}" >/dev/null; sleep 1
python3 tools/tcp_cmd.py 4399 '{"cmd":"quit","id":9}' >/dev/null || kill $WIN
wait $WIN 2>/dev/null || true

cp tests/screenshot-config.ini "$TMP/config.ini"
S="$PWD/$TMP"
LNG_SCRIPT="wait:40;shot:$S/launcher-main.png;view:game;wait:15;shot:$S/launcher-game.png;view:settings;wait:15;shot:$S/launcher-settings.png;view:about;wait:15;shot:$S/launcher-about.png;quit" \
    build/TopGun2 "$ROM" --config "$S/config.ini" >/dev/null 2>&1
for n in launcher-main launcher-game launcher-settings launcher-about; do cp "$TMP/$n.png" "$OUT/$n.png"; done

missing=0
for n in gameplay-intro gameplay-briefing gameplay-flight gameplay-banking gameplay-deck gameplay-mission2 gameplay-mission3 ingame-menu launcher-main launcher-game launcher-settings launcher-about; do
    [ -s "$OUT/$n.png" ] || { echo "MISSING: $n" >&2; missing=1; }
done
[ "$missing" = 0 ] || exit 1
ls "$OUT"

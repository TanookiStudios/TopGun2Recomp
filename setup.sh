#!/usr/bin/env bash
# Fetch NESRecomp and recomp-ui into vendor/ at the versions this port was
# tested with, then apply our local changes to them (patches/).
# vendor/ is not committed: anything changed there must live in patches/.
set -euo pipefail
cd "$(dirname "$0")"
NESRECOMP_COMMIT=2b4fcdb98c9e702be3fb365cfc645003c50b5fc1
RECOMP_UI_COMMIT=41c88ffea852b970af8d50312620b17404e455ad
fetch() {  # fetch <url> <dir> <commit>
    if [ ! -d "vendor/$2/.git" ]; then
        git clone -q "$1" "vendor/$2"
        git -C "vendor/$2" checkout -q "$3"
    fi
}
fetch https://github.com/mstan/nesrecomp.git nesrecomp "$NESRECOMP_COMMIT"
fetch https://github.com/mstan/recomp-ui.git recomp-ui "$RECOMP_UI_COMMIT"
apply() {  # apply <dir> <patch>
    if git -C "vendor/$1" apply --check "../../$2" 2>/dev/null; then
        git -C "vendor/$1" apply "../../$2" && echo "applied $(basename "$2")"
    else
        echo "$(basename "$2") already applied (or does not fit: check vendor/$1)"
    fi
}
for p in patches/recomp-ui-*.patch; do apply recomp-ui "$p"; done
for p in patches/0*.patch; do apply nesrecomp "$p"; done
[ -f rom/topgun2.nes ] || echo "Put your Top Gun: The Second Mission (USA) .nes file at rom/topgun2.nes"

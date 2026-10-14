#!/usr/bin/env bash
# Fetch NESRecomp into vendor/ and apply our local fixes to it.
# vendor/ is not committed: anything changed there must live in patches/.
set -euo pipefail
cd "$(dirname "$0")"
[ -d vendor/nesrecomp/.git ] || git clone --depth 1 https://github.com/mstan/nesrecomp.git vendor/nesrecomp
[ -d vendor/recomp-ui/.git ] || git clone --depth 1 https://github.com/mstan/recomp-ui.git vendor/recomp-ui
for p in patches/recomp-ui-*.patch; do
    git -C vendor/recomp-ui apply --forward "../../$p" 2>/dev/null && echo "applied $(basename "$p")" || echo "$(basename "$p") already applied"
done
for p in patches/0*.patch; do
    [ -e "$p" ] || continue
    git -C vendor/nesrecomp apply --forward "../../$p" 2>/dev/null && echo "applied $(basename "$p")" || echo "$(basename "$p") already applied"
done
[ -f rom/topgun2.nes ] || echo "Put your Top Gun: The Second Mission (USA) .nes file at rom/topgun2.nes"

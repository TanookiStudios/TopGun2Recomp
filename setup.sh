#!/usr/bin/env bash
# Fetch NESRecomp into vendor/ and apply our local fixes to it.
# vendor/ is not committed: anything changed there must live in patches/.
set -euo pipefail
cd "$(dirname "$0")"
[ -d vendor/nesrecomp/.git ] || git clone --depth 1 https://github.com/mstan/nesrecomp.git vendor/nesrecomp
for p in patches/*.patch; do
    [ -e "$p" ] || continue
    git -C vendor/nesrecomp apply --forward "../../$p" 2>/dev/null && echo "applied $(basename "$p")" || echo "$(basename "$p") already applied"
done
[ -f rom/topgun2.nes ] || echo "Put your Top Gun: The Second Mission (USA) .nes file at rom/topgun2.nes"

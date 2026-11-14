#!/usr/bin/env bash
# Fetch third-party sources at pinned commits into build/deps (not committed).
# devkitPro's own servers are unreachable from some networks, so everything comes from GitHub.
set -euo pipefail
DEST=${1:-build/deps}
mkdir -p "$DEST"

fetch() {  # name url commit
    local dir="$DEST/$1"
    if [ -d "$dir/.git" ] && [ "$(git -C "$dir" rev-parse HEAD)" = "$3" ]; then return; fi
    rm -rf "$dir"
    git init -q "$dir"
    git -C "$dir" remote add origin "$2"
    git -C "$dir" fetch -q --depth 1 origin "$3"
    git -C "$dir" checkout -q FETCH_HEAD
    echo "fetched $1 @ ${3:0:7}"
}

# libtonc (MIT)                      - GBA hardware library
fetch libtonc         https://github.com/gbadev-org/libtonc.git     c5af1b2cb019dcde43216596390490bc07800b21
# devkitarm-crtls (MPL-2.0)          - gba_crt0.s + gba_cart.ld (startup code, memory map)
fetch devkitarm-crtls https://github.com/devkitPro/devkitarm-crtls.git 1c0c10257c44bbb5a433453bb6bba91582825492
# gba-tools (GPL-3.0, build tool only, not linked into the ROM) - gbafix header fixer
fetch gba-tools       https://github.com/devkitPro/gba-tools.git    054d507f90d32784274b6cf7e03f1c43d02d7a57
# Unity (MIT)                        - C unit-test framework for host tests (tests/unit), v2.6.1
fetch unity           https://github.com/ThrowTheSwitch/Unity.git   cbcd08fa7de711053a3deec6339ee89cad5d2697

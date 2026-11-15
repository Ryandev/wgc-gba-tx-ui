#!/usr/bin/env bash
# Set up a machine to build and test wgc-gba-tx-ui (Ubuntu 24.04, also under WSL). No Docker, no
# devkitPro servers (they Cloudflare-block some networks): the compiler and C library are Ubuntu's own
# arm-none-eabi GCC + newlib; libtonc, the GBA crt0 / linker script and gbafix come from GitHub at pinned
# commits (scripts/fetch_deps.sh, run by the build).
#
#   bash scripts/setup_ubuntu.sh           install the packages (asks for sudo), fetch deps, build, verify
#   bash scripts/setup_ubuntu.sh --check   only check what's installed (no sudo, changes nothing)
#
# The same packages as CI (.github/workflows/ci.yml). Optional extra: scripts/install_mull.sh.
set -Eeuo pipefail
cd "$(dirname "$0")/.."

PKGS=(
    gcc-arm-none-eabi       # ARM cross compiler (GCC 13.2): the ROM
    binutils-arm-none-eabi  # assembler, linker, objcopy
    libnewlib-arm-none-eabi # C library for the GBA (ARMv4T multilibs)
    build-essential         # host gcc: unit tests, gbafix, the emulator harness
    git                     # fetches the pinned libtonc / crt0 / gbafix sources
    python3-pil             # asset generators (font, controller drawing) and the image tests
    librsvg2-bin            # rsvg-convert: renders assets/controller.svg for the ROM's graphics
    libmgba-dev             # headless emulator library for the timing and screenshot tests
    cppcheck clang-tidy clang-format gcovr # lint, formatting and coverage (make check)
    mgba-qt                 # emulator window, to play the ROM by hand (make run)
)
MGBA_PIN=0.10.2+dfsg-1.1build3 # the version CI uses: other versions may render or time differently

check() {
    local missing=()
    for p in "${PKGS[@]%%#*}"; do
        [ -z "$p" ] && continue
        dpkg -s "$p" >/dev/null 2>&1 && continue
        # Pillow from pip works as well as the apt package, as long as it has FreeType
        if [ "$p" = python3-pil ] && python3 -c "from PIL import features; assert features.check('freetype2')" 2>/dev/null; then
            continue
        fi
        missing+=("$p")
    done
    if ((${#missing[@]})); then
        echo "missing packages: ${missing[*]}"
        return 1
    fi
    local mgba
    mgba=$(dpkg-query -W -f='${Version}' libmgba-dev)
    [ "$mgba" = "$MGBA_PIN" ] || echo "note: libmgba-dev is $mgba, CI pins $MGBA_PIN (goldens could differ)"
    echo "packages: all present"
    echo "  $(arm-none-eabi-gcc --version | head -1)"
    echo "  GBA multilib: $(arm-none-eabi-gcc -mcpu=arm7tdmi -mthumb -print-multi-directory)"
}

if [ "${1:-}" = "--check" ]; then
    check
    exit $?
fi

sudo apt-get update
sudo apt-get install -y "${PKGS[@]%%#*}"
sudo apt-get install -y --allow-downgrades "libmgba-dev=$MGBA_PIN" || echo "note: couldn't pin libmgba-dev to $MGBA_PIN"
check
make deps       # pinned libtonc, crt0 and gbafix sources into build/deps
make            # the ROM: build/wgc-gba-tx-ui.gba
make hooks      # pushes run `make check` first
echo "set up. Run 'make check' for the full test suite (a few minutes), 'make run' to play."

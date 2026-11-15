#!/usr/bin/env bash
# OPTIONAL. Install Mull 0.38.0 (LLVM-based mutation testing, LLVM 18) for wgc-gba-tx-ui on Ubuntu
# 24.04 amd64. Not needed: `make mutate` (tests/mutate.py, our own tool) mutates the real ARM build and
# covers the same code; Mull is the host-side, IR-level alternative (ENGINEERING.md §4, mull.yml).
# Verifies GitHub's published SHA-256 digests before installing.
#   bash scripts/install_mull.sh     (log: /tmp/gba-mull.log)
set -Eeuo pipefail
LOG=/tmp/gba-mull.log
exec > >(tee -a "$LOG") 2>&1
log() { echo "[$(date +%H:%M:%S)] $*"; }
trap 'rc=$?; log "FAILED (exit $rc) at line $LINENO: $BASH_COMMAND"; exit $rc' ERR

BASE=https://github.com/mull-project/mull/releases/download/0.38.0
declare -A PKG=(
  [Mull-0.38.0-ubuntu-amd64-24.04.deb]=83ebc3c49f173b83dc0d991fff4b3ac8d6a2a7ecf51244d598507f85b04d5a29
  [Mull-18-0.38.0-LLVM-18.1.3-ubuntu-amd64-24.04.deb]=8b3840841fc1cea78844ae35a1d48c0a0db3b0ce435019cd50ef78dd578b61bb
)
D=$(mktemp -d)
for f in "${!PKG[@]}"; do
  log "download $f"
  curl -fsSL -o "$D/$f" "$BASE/$f"
  echo "${PKG[$f]}  $D/$f" | sha256sum -c -
done
log "install (clang-18 is required by the Mull plugin)"
sudo apt-get install -y clang-18 "$D"/Mull-0.38.0-ubuntu-amd64-24.04.deb "$D"/Mull-18-0.38.0-LLVM-18.1.3-ubuntu-amd64-24.04.deb
log "verify: $(mull-runner-18 --version 2>&1 | head -1)"
ls -l /usr/lib/mull-ir-frontend-18
log "==== done ===="

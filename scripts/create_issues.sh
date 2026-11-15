#!/usr/bin/env bash
# Create the project's backlog as GitHub issues (run once; safe to re-run: existing titles are skipped).
#
#   sudo apt install gh && gh auth login      # once
#   bash scripts/create_issues.sh             # from the repo root
#
# Every issue points at the spec (SPEC.md letter) it comes from. Phases as in SPEC.md §2.
set -euo pipefail
REPO="${REPO:-Ryandev/wgc-gba-tx-ui}"
command -v gh >/dev/null || { echo "gh not installed: sudo apt install gh && gh auth login"; exit 1; }

labels=("phase-0.5:hardware validation" "phase-1:Phase 1 follow-ups" "phase-2:Phase 2" "phase-3:Phase 3"
        "phase-4:Phase 4 (needs a GBxCart)" "stretch:stretch goal" "companion:Pi-side companion"
        "docs:documentation and release" "needs-hardware:can only be settled on the real cart")
for l in "${labels[@]}"; do
    gh label create "${l%%:*}" --repo "$REPO" --description "${l#*:}" --force >/dev/null
done

existing=$(gh issue list --repo "$REPO" --state all --limit 500 --json title --jq '.[].title')

issue() { # title, labels (comma separated), body (stdin)
    local title="$1" lab="$2" body
    body=$(cat)
    if grep -Fxq "$title" <<<"$existing"; then echo "exists: $title"; return; fi
    gh issue create --repo "$REPO" --title "$title" --label "$lab" --body "$body" >/dev/null
    echo "created: $title"
}

issue "Phase 0.5: validate the ROM on the real cart" "phase-0.5,needs-hardware" <<'EOF'
Everything so far is proven in mGBA only (bus writes and timing match the stock ROM byte for byte; the
emulator doesn't model the cart's CPLD / ATtiny). Before relying on the ROM:

1. Dump the stock ROM with the GBxCart RW and check it against `reference/stock-rom/SHA256SUMS` (FLASHING.md).
2. Flash this ROM. Check: receiver LED solid, `jstest` on the Pi shows every button, no stuck or phantom keys.
3. Probe ROM: send the GB pair ~2 s after key traffic starts. Does the cart switch to GB mode mid-session?
   (decides whether in-session GB mode / pairing can apply without a restart: SPEC §N)
4. Look at the boot flash, black screen and dim on the real (backlit?) screen.
5. Restore the stock ROM, or keep this one.

Refs: SPEC.md §2 (Phase 0.5), FLASHING.md, HARDWARE.md.
EOF

issue "Per-frame CPU budget test (VCOUNT at end of frame work)" "phase-1" <<'EOF'
Performance is enforced by the link timing suite (gap, press-to-send, period, rate) and by goldens that
catch late screens, but there is no direct per-frame budget check: overruns were found indirectly
(missed taps, a white boot frame).

Add: the harness records VCOUNT when each frame's work ends (or a test build writes it to a debug
register); a test fails if any frame overruns or comes within a margin, across the heavy scenarios
(splash, notes, Settings zoom/redraws, idle). Refs: SPEC §0 rule 7, ENGINEERING.md.
EOF

issue "Boot: avoid the ~7 white frames at power-on" "phase-1,needs-hardware" <<'EOF'
crt0 clears RAM with the display force-blanked before main(), so the screen is white for ~7 frames (stock
too). Black-screen users (backlit mods) would prefer black. Needs our own crt0 (set the backdrop / DISPCNT
before the clears) or a faster clear. Decide after seeing it on hardware. Refs: SPEC.md "Boot flash note".
EOF

issue "In-session pairing and GB mode without a restart" "phase-1,needs-hardware" <<'EOF'
Today in-session Settings shows pairing and GB mode read-only ("needs a restart"), because the stock ROM
only ever sends the GB pair / pairing message straight after sync. If the Phase 0.5 probe shows the
ATtiny accepts them mid-session, make them editable in-session. Depends on: Phase 0.5. Refs: SPEC §N.
EOF

issue "E: graphical pairing wizard" "phase-2" <<'EOF'
Replace the pairing page with a 3-step wizard (SPEC §E): pick channel/address/packets (hold to
accelerate), "press A to send" (stock-identical bytes), then an animated "plug in the receiver, look
for 2 blinks, power-cycle" prompt. Lives entirely inside Settings. Mockup: 20_settings_pairing.
EOF

issue "L: sound" "phase-2" <<'EOF'
Optional UI sounds (SPEC §L): menu clicks, splash chime. Must never delay a send (no blocking audio work
in the main loop beyond what the timing suite allows).
EOF

issue "S: Stats page" "phase-2" <<'EOF'
Settings > STATS (SPEC §S, O): packets/s, uptime, press counts, the send rate. In-session only
information; cheap to keep (counters in the send IRQ must stay tiny).
EOF

issue "S / R: Help page" "phase-2" <<'EOF'
Settings > HELP (SPEC §S, stretch R): receiver LED meanings (solid = OK, blinking = weak), boot holds
(Start, Select, Select+Down, A), the L+R+Select combo, troubleshooting.
EOF

issue "H: turbo / autofire" "phase-3" <<'EOF'
Per-button turbo (SPEC §H) on the INPUT page (mockup 22: TURBO A/B/L/R with a rate). Steady marker on
turbo buttons on the main screen (no flicker). The timing suite must cover turbo patterns.
EOF

issue "I: remapping and macros" "phase-3" <<'EOF'
Remap buttons and simple macros (SPEC §I), INPUT > REMAP... Applied before packing the key word in the
send IRQ: keep it constant-time and tiny.
EOF

issue "J: X/Y combo on v1.1a (hardware test)" "phase-3,needs-hardware" <<'EOF'
v1.1b sends L+R+A/B as X/Y. On v1.1a the 7-bit halves have room for the bits, but whether the ATtiny and
receiver pass them is unknown. Test with the receiver on a PC; then expose an INPUT toggle (SPEC §J).
EOF

issue "M: input tester" "phase-3" <<'EOF'
A Settings page that shows raw key state and timing without sending (SPEC §M), for checking a worn pad.
EOF

issue "F: save settings in the cart's flash" "phase-4,needs-hardware" <<'EOF'
Keep options and pairing across power-off in a spare sector of the cart's 4 MB NOR flash (SPEC §F
research): write from RAM with IRQs off, only from boot-time Settings (the ATtiny ignores the bus before
sync), two alternating sectors with a checksum, encoded so no byte reads as the sync value. Needs a
GBxCart on hand to recover. Unlocks THEME (U).
EOF

issue "U: colour presets (theme)" "stretch" <<'EOF'
Configurable UI from a preset list (SPEC §U): drawing / background colours. Palette-only (cheap). Needs
saves (F) to be worth it.
EOF

issue "O / P / Q: session stats, accessibility, build profiles" "stretch" <<'EOF'
Stretch goals from SPEC.md: O session stats, P accessibility and display modes, Q build-time "profile"
ROMs. Split into separate issues when picked up.
EOF

issue "Z: Pi-side link monitor" "companion" <<'EOF'
The cart can't know whether the receiver hears it (one-way link). A small Pi tool could watch the
receiver's HID reports and show real link health (SPEC §Z). Investigate first: does the receiver keep
reporting every 8 ms when idle?
EOF

issue "Release: packaging and user guide" "docs" <<'EOF'
SPEC.md §2.5: release zip with both ROMs (v1.1a / v1.1b), LICENSE, LICENSES.md and assets/fonts/OFL.txt;
a short user guide (boot holds, Settings combo, LED meanings, flashing with FlashGBX, restoring stock).
No insideGadgets files. Say "for the insideGadgets Wireless Gameboy Controller", unofficial.
EOF

issue "LICENSES: record the source of the CAD's reference GBA.STEP" "docs" <<'EOF'
The controller drawing is traced from the owner's CAD, whose part placement refers to a reference
`GBA.STEP`. Record its source and licence in LICENSES.md (placeholder there now).
EOF

issue "Verify CI on GitHub Actions" "docs" <<'EOF'
The pre-push hook runs `make check` locally; confirm the Actions workflow (.github/workflows/ci.yml) is
green on GitHub too (python3-pil and librsvg2-bin were added for the asset generators).
EOF

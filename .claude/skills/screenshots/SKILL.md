---
name: screenshots
description: Generate, compare and approve screenshots of this ROM in the headless emulator (mGBA). Use when the owner asks to see a screen, when comparing the ROM with a mockup, when a visual test fails, or before approving new or changed goldens. Covers the capture tool, key/timing recipes for every screen, mockup comparisons and the golden-approval rules.
---

# Screenshots of the ROM (runbook)

Everything runs headless in mGBA through `tools/emu_harness.c`; no window needed. For playing it
yourself: `make run` (mgba-qt; X = A, Z = B, arrows, Enter = Start, Backspace = Select, A/S = L/R).

## 1. One-off captures: `tools/shots.py`

    make && make build/emu_harness
    python3 tools/shots.py build/shots/NAME  STEP... [--mockup mockups/X.png] [--scale 2] [--dirty]

Writes `NAME.png` per capture, `sheet.png` (all, labelled) and with `--mockup` a side-by-side
`vs_mockup.png`. v1.1b build: `--rom build/v11b/wgc-gba-tx-ui.gba` (`make build/v11b/wgc-gba-tx-ui.gba`).

**Steps** (harness syntax): `k:KEYS` hold keys (`A B` buttons, `s` Select, `S` Start, `> < ^ v` D-pad,
`r l` shoulders, `-` none), `f:N` run N frames (60 = 1 s), `p:NAME` capture. Keys before the first `f:`
are **held at boot** (boot holds: `S` Settings, `A` GB mode, `s` screen off, `sv` black). `dirty` starts
with garbage VRAM like real hardware.

## 2. Recipes (timings that work)

| Screen | Steps |
|---|---|
| Splash: fade / power-on / title / tip | `f:28 p:fade f:20 p:power f:40 p:title f:60 p:tip` |
| Main screen (after the splash) | `f:200 p:main` |
| Skip the splash with a press | `f:20 k:B f:2 p:skip` |
| Buttons lit | `f:200 k:Av< f:7 p:lit` (wait >= 7 frames after a change: the release fade is 6) |
| GB mode, quiet then L tapped (note, strike) | `k:A f:30 k:- f:200 p:quiet k:l f:7 p:note` |
| In-session Settings (hold L+R+Select 1 s) | `f:200 k:lrs f:61 p:zoom_start f:5 p:zoom_mid k:- f:20 p:home` |
| A Settings page | from home: `k:v f:3 k:- f:3` per row down, `k:A f:3 k:- f:3`, then `p:page` |
| Leave Settings | `k:B f:3 k:- f:3` (to home), `k:B f:3 k:- f:40` (out to the controller) |
| Boot-time Settings | `k:S f:30 k:- f:3 p:home` (nothing is sent until PLAY) |
| Idle dim / asleep | `f:200 f:1815 p:dimming f:30 p:dim f:5400 p:asleep k:B f:3 p:woken` |

Home rows: PLAY, PAIRING, DISPLAY, INPUT, ABOUT. Taps: hold >= 2 frames and release >= 2 frames.
The idle counter starts when the splash ends (about frame 190).

## 3. Comparing with a mockup

Mockups (`mockups/`, git-ignored, owner-approved) are 720 x 480 = the 240 x 160 screen at 3x. Compare
at the same size (`--mockup`), and for detail crop and enlarge both the same way. Measure rather than
eyeball: sample pixel rows/columns with Pillow (bars, text rows, x positions) and match those numbers.
Known, accepted differences from the mockups: 5 speaker slots, wider START/SELECT recesses, smaller LED
(all from the owner's CAD), the right strike mirrored, text 1 px lower in the splash title.

**Never overwrite or regenerate a mockup** the owner has seen; write new files elsewhere (build/).

## 4. Golden images (pixel-exact tests)

`make visual` runs `tests/visual.py`: every scenario in `SCREENS`, both protocols, compared pixel for
pixel with `tests/visual/golden/<ver>/`. Captures named `*_colours` compare only the set of colours
(boot transition frames, whose scanline moves with code size).

Rules (learnt the hard way):
1. **Look before approving.** Open the `build/visual/<ver>/<name>.actual.png` (and `.diff.png`) of every
   failing capture; make a contact sheet if there are many.
2. **Approve by name only:** `make visual-approve SCREEN=a,b` with exactly the screens you checked.
   Never a blanket `make visual-approve`: it once accepted a regression (boot frame 8 white).
3. Boot frames (`boot__frame7` white, `boot__frame8` = the splash's first frame) failing means boot got
   slower: find the cause (usually a big `.bss` buffer crt0 now clears: use `.noinit`), don't approve.
4. Explain every expected difference (which change caused it) before approving; anything you can't
   explain is a bug until shown otherwise.

## 5. Showing the owner

Publish nothing; put the PNGs in `build/` and show them (Read the image) with a one-line caption each.
For before/after, put both side by side in one image.

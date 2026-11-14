# Stock UI (v1.1a) features, and what we might want instead

The screenshots are from the **real stock v1.1a ROM** running in mGBA (headless, via
`tools/emu_harness.c`) on 2026-11-14. They're scaled 3x in `reference/screenshots-v1.1a/`, which is local only: they show insideGadgets' UI, so they aren't published. Run `tools/emu_harness.c` against your own stock ROM dump to regenerate them. mGBA
skips the BIOS boot logo, so you won't see the Nintendo logo here.

## 1. Every screen the stock ROM has

### Normal boot (nothing held)
`01_normal__02_keyheld.png`

- The title "insideGadgets" plus "Wireless GBA Controller v1.1a", in white libgba console text on
  dark grey (`#424242`).
- One `.` at row 3 while **any** key is down. That's the only feedback that anything is happening.

### B held at boot: key letters
`02_showkeys__05_L_R.png` `02_showkeys__04_start_select.png`

- Prints letters for the keys held: `L R U D A B S T L R`. Left and the L shoulder both print `L`,
  and Start/selecT print `S`/`T`, so it's ambiguous.

### Start held at boot: Options (pairing) menu
`03_config__01_menu.png` `03_config__05_transmitting.png`

- Three rows: RF Channel, RF Address, Pcks/Refresh. Up/Down moves the `>`, Left/Right changes the value.
- **Holding Left or Right auto-repeats fast.** Two short presses moved the channel from 1 to 7 in the test.
- The values always start at 1/1/1. The ROM can't read what the cart actually has stored.
- A sends the config once and shows "Transmitting...". After that you power-cycle by hand.
- The instructions are four lines of plain text.

### Other boot options
| Held at boot | Screen | Notes |
|---|---|---|
| Select | blank dark grey | No text. 1.9 ms send timer instead of about 2.1 ms. |
| Select + Down | blank black | For backlit and IPS screens |
| A | `GB mode` on row 2 | Sends `63 5A` and drops L/R |

## 2. Confirmed in the emulator (cart-bus write log)
| Action | Bytes written to `0x08000000` |
|---|---|
| Boot | `49` |
| GB mode | `49 63 5A` |
| Config (ch 7, addr 1, pcks 1) | `49 6D 52 07 01 01` |
| B / A / Up+Right | `00 02` / `00 01` / `00 14` |
| Start+Select | `01 40` |
| L+R, and L+R+A | `06 00`, `06 01` (no X bit in v1.1a) |
| Rate | about 7–8 key pairs per frame, so one packet every ~2.1 ms (screen off: ~1.85 ms) |

That matches `HARDWARE.md` §4. These logs will be the regression test for any new ROM.

## 3. What the stock UI doesn't do (opportunities)
- No real picture of the controller. A single `.` is the only sign of input.
- No feedback that it's sending. There's no TX indicator, packet counter or rate. Everything
  happens invisibly at 500 Hz.
- Pairing is fiddly: you have to remember your channel and address, the menu starts at 1/1/1, and
  there are no guided steps.
- The boot options (Select, Select+Down, A, B, Start) are hidden and you only find them in the manual.
- No theming, sound or splash screen, and it has no visual connection to the giant GBA.
- No power awareness: the screen is either full on or off, with no dimming or sleep after idle.

## 4. Feature menu for the new ROM

All agreed (2026-11-14). The full spec, the extra features N–R, the phases and the ruled-out ideas
are in **`SPEC.md`**. Original list:

**Core (keep behaviour identical):** v1.1a protocol, 2 ms timer IRQ, every boot option still
works, and the Start-at-boot pairing still works.

**Visual**
- A. A live GBA drawing where each button lights up as you press it (D-pad, A/B, L/R, Start/Select)
- B. A giant-GBA theme: same palette and logo as the build, plus a boot splash or animation
- C. A TX activity light and a packets/sec readout, like a signal meter
- D. A clean status bar: channel, address, mode (GBA or GB), version

**Pairing / settings**
- E. A graphical pairing wizard with guided steps (press A, plug in receiver, power cycle) and animations
- F. A remembered last-used channel and address. The cart has no save RAM that we know of, so this
  needs investigating. It may not be possible.
- G. A visible menu for the boot options (screen off, black, GB mode) instead of hidden key combos

**Input extras (ROM-side only, the receiver doesn't change)**
- H. Turbo or autofire per button (it's on insideGadgets' own wish list)
- I. Macros or button remaps done on the GBA before sending
- J. An X/Y combo (L+R+A/B) as an experiment, since it's unknown whether a v1.1a ATtiny forwards it

**Power / misc**
- K. Dim, then blank, the screen after N seconds idle, and wake on any key
- L. Sound: a click on button press or a chime on pairing. Off by default, because the speaker
  already buzzes from the RF.
- M. An input tester or latency mode, handy when setting up RetroArch on the Pi

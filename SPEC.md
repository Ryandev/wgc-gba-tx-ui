# New cart UI ROM: feature spec

Status: **draft, agreed in principle 2026-11-14**. All the features below are in scope. The phases
set the build order. Protocol facts are in `HARDWARE.md`. The stock behaviour we must keep is in
`FEATURES.md`.

---

## 0. Ground rules (apply to every feature)

1. **The link comes first.** Key packets go out from the timer-3 IRQ every 2 ms, whatever the UI is
   doing. Rendering, sound and menus never delay or drop a packet. The byte stream must match the
   stock logs in `tests/baselines/v1.1a/` exactly for the same inputs.
2. **Protocol target is v1.1a** (sync `0x49`, 7/7 split). It's a build flag, `CART_PROTOCOL=V11A|V11B`.
3. **The cart bus is off limits** apart from protocol writes to `0x08000000`. WAITCNT stays at the
   BIOS default. Hot code runs from IWRAM.
4. **Stock compatibility.** The boot holds Start / Select / Select+Down / A / B behave as the manual
   says, so the insideGadgets docs stay valid.
5. **Input ownership.** Core menus only exist at boot, before gameplay. If the stretch overlay (N) is
   built, its presses must **not** reach the Pi: while it's open the cart sends "no keys pressed".
6. Toolchain: devkitARM + **libtonc** (C). It's light, gives direct IRQ/register control, and has no
   `printf` bloat. Assets come from `grit`. Test in mGBA with `tools/emu_harness.c` before going
   anywhere near hardware.

7. **Performance is a core requirement (owner, 2026-11-14).** The UI must never add input latency.
   Button → cart write is at most one send period (2 ms) plus the write itself, whatever is on screen.
   - **No large DMA during play.** A DMA transfer stalls the CPU and IRQs. 32 KB from ROM is about
     5 ms, which would delay a send. Bulk graphics load at boot, the splash, or behind the Settings
     zoom (input is paused there anyway). During play: palette swaps, a few tiles, OAM updates,
     each well under 0.2 ms.
   - **Keep IRQ handlers tiny.** libtonc's default master ISR doesn't nest, so a long VBlank handler
     would hold back the timer-3 send. VBlank only sets a flag or copies a shadow OAM. All drawing
     runs in the main loop, which the send IRQ can preempt. The send IRQ has priority. If any
     handler grows, switch to the nesting ISR (`isr_master_nest`).
   - **Enforced by tests** (the harness logs a CPU-cycle timestamp per cart write):
     - *Gap test:* during a UI-heavy scenario (splash, zoom in/out, LCD notes, idle dim), no gap
       between consecutive key words exceeds 2.2 ms (stock: about 2.1 ms).
     - *Press-to-send test:* a key change appears in the cart writes within 2.2 ms of the
       KEYINPUT change, measured in cycles.
     A failure blocks the merge, like a protocol mismatch.

---

## 0.5 Screen layout principles (agreed 2026-11-14)

The **main screen shows only two things**:
1. **What's pressed**: the controller drawing (A) fills the screen.
2. **Link status**: a small status icon (C). It's quiet when everything is normal, and turns into
   a one-line banner when there's something to flag.

Everything else goes on the **Settings / Stats** pages (S): channel, address, firmware and protocol
version, packet rate, stats, tester and help. Nothing technical stays on screen permanently.

**Honesty rule:** the cart can't know whether the receiver hears it. The link is one-way: nRF ACK
is off, and the CPLD only captures writes, so nothing comes back to the GBA. The UI never shows
"connected", "signal strength" or "signal lost". It only flags states the cart really knows (C),
and it points the user at the **receiver LED** (solid = OK, flicker/dim = packet loss), which is the
true link indicator. Real end-to-end status comes from the Pi side (Z).

---

## 1. Features

Each one has its behaviour, notes and an acceptance check. Letters match `FEATURES.md`.
Phase = build order (see §2).

### A. Live controller view (Phase 1)
- A full-screen GBA drawing (front view including the shoulder buttons). Each button lights up
  while pressed, with a short "release" fade.
- **Looks like a real AGB-001**: generated from the CAD front outline (`../Designs/dimensions/GBA_Front.dxf`
  via `tools/gba_front_labels.py`; see `mockups/09_*`), and
  **no text labels**. Buttons are recognised by where they are. All 10 inputs, including L/R and
  Start/Select, light up individually.
- Built from sprites over a BG layer. Updates on VBlank from the same key state the IRQ sent.
- *Accept:* every one of the 10 buttons lights within 1 frame of press, and chords (e.g. L+R+A+Up)
  show correctly.

### B. Theme and boot splash (Phase 1 splash, revised 2026-11-14)
**Splash storyboard** (`mockups/31_splash_storyboard_v2.png`). Chosen: the v2 frames (fade-in with LED off = `29_*`). It's original, with no imitation
of Nintendo's boot sequence. About 3 s in total, and any key skips it:
1. The GBA fades in from the background, with the LED off.
2. The power LED switches on green (with glow) and the LCD lights up green.
3. The LCD shows an antenna icon, "WIRELESS / CONTROLLER", and the version.
4. A tip on the LCD, "SETTINGS: HOLD L + R + SELECT FOR 1 SEC", with L, R and Select lit on the drawing.
Then the main screen. **Keys are sent from frame 1.** The splash never delays play, and a
key-skip press still reaches the Pi.

Original notes:
- A palette and logo that match the giant GBA build, plus a ~2 s splash animation on boot. Any key
  skips it, and the boot holds still work.
- The link starts **before** the splash, so you can play straight away.
- Colours are a free choice for the **controller GBA's own screen**. They're independent of the
  giant GBA shell. Proposed palettes are in `mockups/`.
- *Accept:* the splash never delays the first packet (check the cart-write log: sync arrives within
  the first frames).

### C. Status: power LED plus LCD notes (Phase 1, revised 2026-11-14)
The drawn GBA's **power LED is the status light**, and **notes appear as multi-line text on the
drawn LCD** (green LCD, dark ink, 12 chars × 7 lines). There's no banner and no corner icon. The
controller is vertically centred. See `mockups/12_*`–`15_*`.

LED colours only mean what the cart can actually know. The link is one-way (§0.5), so the GBA
can't measure connection quality, and amber/red don't mean "weak/lost signal".

**The main screen is status only.** It never stands in for anything under Settings. Pairing,
including its "power cycle to play" step, happens entirely in Settings (E/S).

| LED | Meaning | States | LCD note |
|---|---|---|---|
| **Green** | Sending your input normally | Normal play | blank |
| **Amber** | Sending, with a caveat | GB mode (L/R not sent); a key held ≥ 60 s (warning off by default) | GB mode: **nothing** until L or R is pressed (see below). Others: a short timed note, e.g. "CHECK THE RECEIVER LED / SOLID = OK / BLINK = WEAK". |
| **Red** | Not sending gameplay input | Reserved. Only reachable from the main screen if the stretch overlay (N) pauses sending. | e.g. "PAUSED / INPUT NOT SENT" |

**GB mode is quiet:** the LED is amber and nothing else changes. When L or R is pressed, the
shoulder lights **amber** (pressed but not sent, instead of the usual highlight), the other
shoulder is struck through, and the LCD shows "GB MODE / L/R NOT SENT / REBOOT WITHOUT A" with a
countdown bar. It all hides after about 3 s from the last L/R press. See `mockups/16_*`, `18_*`.

- Notes can't be dismissed with a button (presses go to the Pi). They're timed and auto-hide, and
  the LED keeps showing the state.
- Real link quality: the receiver LED (solid = OK, flicker/dim = loss), or the Pi monitor (Z).
- *Accept:* each state shows the right LED colour and note in the harness. Normal play = green and
  a blank LCD.

### D. ~~Status bar~~ → info lives in Settings / Stats (S)
- Channel, address, mode, protocol and ROM version are **not** on the main screen any more.
  They're on the About and Pairing pages.
- Channel and address are the *assumed* values (build-time defaults, see F), because the ATtiny's
  EEPROM can't be read back.

### S. Settings / Stats pages (Phase 1 basic, Phase 2 full)
- Opened by holding **Start at boot** (as stock), or from the stretch overlay (N) if that's built.
- Pages:
  - **Pairing:** the wizard (E)
  - **Display:** screen off, black screen, idle dim (G, K)
  - **Input:** turbo, remap, macros, X/Y (H, I, J)
  - **Stats:** packets/s, uptime, press counts, input tester (M, O)
  - **Help:** receiver LED meanings, boot holds, troubleshooting
  - **About:** ROM and protocol version, assumed channel/address/packets, and **"FREE + OPEN
    SOURCE" with the GitHub repo URL**: `github.com/Ryandev/wgc-gba-tx-ui`, on two lines on screen
    (`GITHUB.COM/RYANDEV/` + `WGC-GBA-TX-UI`). Layout in `mockups/32_*`.
    The send rate moves to Stats.
- **Boot order (fixed after Gemini review F2, 2026-11-14):** read the keys **before** sending
  anything.
  - **Start not held** (normal boot): sync, GB pair if GB mode, start sending, *then* the splash.
    Keys are live from the splash's first frame.
  - **Start held** (boot-time Settings): **no sync, no splash, nothing sent.** The ATtiny is still
    waiting for sync and ignores the bus, which makes this the only safe moment for flash saves (F).
  - **PLAY** (or Start) from boot-time Settings: sync, then the GB pair if GB mode is on, start
    sending, zoom out to the main screen.
  - **Pairing** (A in the wizard): `link_send_config()` sends **sync first**, then `CFG1 CFG2 ch addr
    pk`, exactly as stock (baseline 03_config: `49 6D 52 07 01 01`). PLAY is then replaced by "POWER
    CYCLE TO PLAY", because the ATtiny repeats the config until power-off.
  - **In-session Settings** (L+R+Select): sync was sent long ago, so changes apply to RAM for this
    session only. Pairing and GB mode show "applies at next boot", and nothing is saved to flash.
- **Zoom transition (2026-11-14):** entering Settings zooms into the drawn LCD (centred on it)
  until it fills the screen at about 2.33×, then the menu fades in on the green. Exiting is the
  reverse: menu fades out, zoom back out to the controller. The drawn LCD's aspect (about 1.48) is
  almost exactly the screen's (1.5), so the LCD *becomes* the Settings screen. Its green must
  match the Settings background exactly. Implementation: the controller drawing on an affine BG
  (mode 1, BG2), about 12 frames / 200 ms, LED red from the first frame (neutral is sent as soon
  as the combo fires). See `mockups/33_settings_zoom_storyboard.png`. On boot-time entry (Start
  held) the same zoom runs straight after the splash.
  **Layer plan (Gemini F8):** affine BG2 (mode 1) is 8bpp, one 256-colour palette, max 256 tiles
  (the controller needs about 189), no tile flips, and doesn't scale sprites. So the controller and LED live
  in BG2. Pressed-button highlights are sprites (OBJ) during play and are **hidden on zoom frame 1**
  (input is paused then anyway). The LED colour is a BG2 palette entry. Settings text is BG0,
  faded in with alpha blending.
- GB mode is also a toggle under Input. Holding A at boot still works.
- Without storage, the hub says "CHANGES LAST UNTIL POWER OFF".
- The full-screen green-LCD style is in `mockups/19_*`–`23_*`.
- The stock B-at-boot "key letters" option is dropped, because the live drawing replaces it.

**Decided 2026-11-14:**
- "Key held" warning: **off by default**. When enabled the threshold is 60 s.
- Turbo on the main screen: a **steady marker** on turbo-enabled buttons (no flicker, which would
  look like a fault).
- The power LED gets a **soft glow** (1–2 px halo) so its status reads at a glance.
- **In-session entry: hold L+R+Select for 1 s** opens Settings (N, promoted from stretch to core).
  Accepted side effect (owner, 2026-11-14): the game receives L+R+Select for that second (about 500
  key words) before input pauses. Suppressing it would mean delaying every L/R/Select press.
  Holding Start at boot still works.

### E. Pairing wizard (Phase 2)
- Replaces the Start-at-boot menu with a graphical one. Holding Start at boot still opens it.
- Step 1: pick the channel, address and Pcks with nicer widgets. Hold to accelerate, with
  controlled repeat rather than the stock runaway repeat.
- Step 2: "Press A to send". Sends the same `6D 52 ch addr pk` sequence as stock.
- Step 3: an animated prompt to plug in the receiver ("look for 2 blinks"), then "power-cycle the
  GBA".
- **Lives entirely inside Settings.** After A is pressed the ATtiny sends the config until
  power-off, so the wizard's last screen ("POWER CYCLE TO PLAY") stays up and never returns to the
  main screen.
- v1.1b build only: limit **channel and address** to 1–63 (`PROTO_MAX_CH/ADDR` in `src/protocol.h`;
  see `HARDWARE.md` §4.4). A 6-bit path would also drop bit 6 of channels ≥ 64.
- *Accept:* the bytes match the stock config log for the same values.

### F. Remember channel and address (Phase 1 simple / Phase 4 research)
- **Simple (Phase 1):** set the defaults at **build time** (`-DDEFAULT_CH=… -DDEFAULT_ADDR=…`).
  The wizard and status bar then start from *your* pairing instead of 1/1/1.
- **Research (Phase 4): settings saved in the cart's own NOR flash** (checked 2026-11-14).
  The cart has **no save chip** (no SRAM/FRAM/EEPROM on the GBA bus). The ATtiny's EEPROM holds
  ch/addr/pcks but can't be read by the GBA (one-way bus). Plan:
  - Use the last two 64 KB sectors of the 4 MB MX29LV320 (bottom-boot, so the small sectors at
    0x000000 hold the ROM and are never touched). A/B slots with sequence number and checksum, so a
    power cut mid-erase falls back to the previous copy.
  - The erase/program routine runs from IWRAM with IRQs off, because flash can't be read while
    busy. JEDEC commands in word mode: GBA `0x08000AAA` / `0x08000554`.
  - **ATtiny safety:** every cart write also reaches the ATtiny, but it ignores everything until
    the sync byte (`0x49` on v1.1a). So **save only in the boot-time Settings screen, before sync is
    sent**, and encode the data so **no 16-bit halfword written** (word-mode program data *and* the JEDEC
    command words) has its low 7 bits = `0x49` (low 6 bits = `0x29` on v1.1b). The CPLD latches
    AD0–6 of every write. JEDEC words AA/55/A0/80/30 → 2A/55/20/00/30 are safe. Gemini review F2. Gameplay
    never coincides with a flash write.
  - **Drain the CPLD after flashing** (NEEDS HARDWARE, Gemini F2.2): status polling (DQ7/DQ6) is cart
    reads, which re-arm the CPLD between writes, and the last write leaves its ready line set. After
    the final operation, do a few dummy ROM reads and the stock delay before `link_sync()`.
  - Unverified: that /WR reaches the flash's WE# through the CPLD for GBA-initiated writes (likely,
    since GBxCart RW flashes it in place). mGBA treats ROM as read-only, so this needs a hardware
    test with a GBxCart RW for recovery. Until then, use build-time defaults (simple F).
  - Candidates to persist: theme, idle timeouts, turbo/remap/macros, last-used ch/addr/pcks (so the
    pairing wizard and About show real values).

### G. Visible boot-options menu (Phase 1)
- A Settings screen listing screen-off, black screen, GB mode and key letters, as toggles.
  It's opened from the boot-time settings screen, or from the overlay if stretch N is built.
- Without persistence (F), the toggles last until power-off. The stock boot holds still work.

### H. Turbo / autofire (Phase 3)
- Per-button turbo for A, B, L, R at selectable rates (e.g. 10/15/20/30 Hz, synced to the 2 ms
  tick). Toggled in the overlay. The button icon shows a turbo badge.
- Note: the receiver polls USB every 8 ms and RetroArch polls once per frame, so turbo above about
  30 Hz will alias.

### I. Remapping and macros (Phase 3)
- Remap: any GBA button to any packet bit (e.g. swap A/B, or turn L into Select). Presets:
  "Stock", "SNES-ish", "Left-handed".
- Macros: record or define short sequences (e.g. a ↓↘→+A motion), triggered by a chord.
- All of it happens on the GBA before packing, so the receiver doesn't change.

### J. X/Y combo, experimental (Phase 3)
- L+R+A → bit 10, L+R+B → bit 11, as in v1.1b. In a v1.1a build bits 10/11 travel in the 7-bit
  high half (`W >> 7`).
- **Unknown:** whether the v1.1a ATtiny and the USB receiver pass them through. It's off by
  default and needs a hardware test with `jstest` on the Pi.

### K. Idle dim and sleep (Phase 1)
- After N s (30 by default) with no input, fade the palette to dim. After M s (120 by default) go
  to a blank or black screen. Any key wakes it. That wake press is still sent as normal: we never
  swallow gameplay input.
- Packets keep flowing while the screen is blank.

### L. Sound (Phase 2)
- Optional button clicks and a pairing chime, **off by default**. The speaker already buzzes from
  the RF bursts.

### M. Input tester (Phase 3)
- A diagnostics screen showing the raw `KEYINPUT`, the packed 12-bit word, the two bytes as sent,
  the per-button press counts and the time since the last change. Handy for setting up RetroArch
  autoconfig on the Pi and for chasing faults.

---

## Companion workstream

### Z. Pi-side link monitor (separate from the ROM; after Phase 1)
- The only place real link health can be seen is the Pi. A small daemon on the Pi watches the
  receiver (`16C0:27DC`, hidraw/evdev). If reports stop or the device drops, it shows a notice on
  the giant screen (RetroArch notification or framebuffer overlay).
- **Investigate first:** does the receiver keep sending HID reports every 8 ms when idle, or only
  on change? What does it do at its packet timeout (2–5 s, then a cart re-scan)? Can a silent link
  be told apart from "no buttons pressed"? Test with `jstest`, `evtest` and `hidraw` dumps while
  switching the GBA off.
- Not part of the cart ROM. It lives under `../` (the Pi software) once designed.

---

## Stretch goals (suggested 2026-11-14, not core)

These are nice-to-haves. Core scope is A–M. Without N, the settings for G/H/I/M live on the
boot-time settings screen (hold Start at boot). N later upgrades them to in-session.

### N. In-session Settings entry and input ownership (**core**, promoted 2026-11-14)
- Holding **L+R+Select for 1 s** opens the overlay. The chord was picked to avoid RetroArch's usual
  Select+Start hotkey. The hold time stops it triggering during play.
- While the overlay is open the cart sends **neutral** (no keys pressed), so menu navigation never
  reaches the game. Closing it restores pass-through.
- The overlay has: Settings (G), Turbo (H), Remap/Macros (I), Tester (M), Theme, About.
- *Accept:* in the harness log, no non-zero key bytes appear while the overlay is open.

- **Finding from Phase 0 (2026-11-14):** the stock ROM always sends the GB-mode pair or the
  pairing message as the *first message straight after the sync byte*, never later. The ATtiny
  most likely only checks the first pair after sync. So, unless a hardware test shows otherwise,
  **pairing and GB-mode changes need a power cycle**: in-session Settings shows "applies at next
  boot" for those two. Pairing itself works from boot-time Settings, because the sync byte isn't
  sent until the user picks PLAY or sends pairing.
- **Mid-session risks (to verify on hardware):** the ATtiny is already synced, so (1) pairing
  sent mid-session relies on the ATtiny accepting the config header after sync. That's likely,
  since key high-halves on v1.1a are only 0–7 and can't collide with `0x6D`, but it's unverified.
  (2) Flash saves mid-session would put command and data bytes on the bus that the ATtiny reads as
  keys. Until tested: in-session changes apply immediately, and **saving to flash happens only from
  boot-time Settings** (hold Start).

### O. Session stats (stretch)
- Uptime, total presses and a per-button heat-map on the controller drawing. Shown in About or the
  tester. Just for fun, and nearly free.

### P. Accessibility and display modes (stretch)
- A high-contrast palette, a large-glyph status bar, and a "dim palette" option for backlit/IPS
  screen mods (a softer version of Select+Down black).

### Q. Build-time "profile" ROMs (stretch)
- A Makefile target that bakes in channel, address, theme and defaults, e.g.
  `make PROFILE=giantgba`. Combined with F (simple) this replaces most of what persistence would do.

### U. Configurable UI from presets (stretch, needs storage from F-research)
- Pick-from-list only (no free colour picking): background colour, GBA body colour (e.g. indigo,
  Glacier clear, Fuchsia, Black, Platinum, Orange, original-style), LCD tint (green, grey, backlit
  white), button highlight colour.
- Each preset is a palette swap: the drawing is palette-indexed, so it costs nothing at runtime.
- Saved with the other settings in the flash settings sector (F). Without storage it falls back to
  the build-time choice (Q).

### R. Self-documenting help screen (stretch)
- A short on-cart help page covering the boot holds, the overlay chord, the pairing steps and the
  LED meanings. It replaces the manual for day-to-day use.

---

## 2. Phases

| Phase | Contents | Notes |
|---|---|---|
| **0** | Protocol core: timer-3 IRQ sender, boot holds, v1.1a/b flag, minimal text screen | Must reproduce all 6 stock logs byte for byte in the harness |
| **1** | A, B (splash), C, F-simple, G, K, N, S (basic) | First "interesting" ROM. Visual only, zero link risk. |
| **2** | B, E, L, S (full) | Theme, splash, pairing wizard, polish |
| **3** | H, I, J, M | Input extras and diagnostics. J needs a hardware test. |
| **4** | F-research (flash persistence) | Only with a GBxCart RW on hand for recovery |
| Stretch | O, P, Q, R, U | Any time after Phase 1 if wanted |
| Companion | Z (Pi link monitor) | After Phase 1. Starts with the receiver investigation. |

Hardware milestones (**changed after Gemini review F6, owner agreed 2026-11-14**): buy the GBxCart RW
**now**. **Phase 0.5 comes before Phase 1:**
1. Dump and verify the stock ROM (`FLASHING.md`).
2. Flash the Phase 0 ROM and confirm the IRQ-driven sender works with the real CPLD + ATtiny
   (receiver LED solid, `jstest` on the Pi shows every button, no stuck or phantom keys).
3. Probe ROM: send the GB pair (`63 5A`) about 2 s *after* key traffic starts. Does the cart switch
   to GB mode? This settles whether GB mode and pairing work mid-session (§N finding).
4. Restore the stock ROM, or keep Phase 0.

---

## 2.5 Open-source release

Repo: **https://github.com/Ryandev/wgc-gba-tx-ui** (created 2026-11-14). Licence: MIT for code, CC BY 4.0 for docs
(`LICENSE`, `LICENSES.md`, decided 2026-11-14).

The new ROM, tools and docs will be published free and open source on GitHub. The repo and
licence are still to be decided. Before publishing:
- **Leave out vendor files.** `reference/stock-rom/*.gba` (insideGadgets binaries), the manual PDF
  and `reference/photos/*` are insideGadgets material (CC BY-NC / NC-SA, or no stated licence).
  Link to the insideGadgets downloads instead of re-hosting them. The SHA-256 list can stay.
- **Font:** the mockups use the libgba console font lifted from the stock ROM. The real ROM should
  use a font with a clear licence (e.g. libtonc's built-in font, or an original one) and must not
  ship the extracted bytes.
- **Naming:** say "for the insideGadgets Wireless Gameboy Controller", without their logo, and
  make clear it isn't official. Credit insideGadgets for the hardware and protocol design.
- The reverse-engineered protocol notes (`HARDWARE.md`) are our own write-up and fine to publish,
  with the source links kept.
- Check that no personal details (emails, local paths) end up in commits or docs.

## 3. Considered and ruled out (so we don't revisit them)

| Idea | Why not |
|---|---|
| True signal strength / receiver-connected indicator *on the GBA* | The GBA can't read anything back from the cart (use the receiver LED, or Z on the Pi). The CPLD only captures writes, and the nRF has ACK disabled. It would need new cart hardware (cf. blog Part 3, which needed a GB-cart rewire). |
| Showing the receiver's or Pi's state on the GBA screen | Same reason: the link is one-way. |
| Changing channel on the fly | The receiver only accepts new config in its first ~0.5 s after power-up, so a replug is always needed. The wizard (E) guides you through that instead. |
| Battery level | The GBA has no battery ADC. |
| Rumble | Needs the hardware mod plus custom ATtiny and receiver firmware (rumble bundle in `reference/photos/`). Out of scope. |
| A playable mini-game while connected | Every press would also go to the Pi. It could go in the overlay (N) with sending paused, but it's low value. |

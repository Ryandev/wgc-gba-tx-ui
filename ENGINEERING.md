# Engineering practices: C on the GBA, linting, testing

Status (2026-11-14): **§1 and §2 adopted** (owner: layered linting rather than full MISRA). `make lint`,
`make check`, CI on every push, and a pre-push hook are in place. §3 to §5 are agreed direction:
host unit tests, mutation testing and golden-image tests get built in Phase 1.
Performance is a core requirement: see SPEC.md §0 rule 7.

---

## 1. C on the GBA: do's and don'ts

The GBA has a 16.78 MHz ARM7TDMI with no FPU and no hardware divide, little fast memory, and
quirky memory buses. Our extra constraint: every ROM fetch goes through the cart's CPLD, and
WAITCNT stays at the BIOS default (HARDWARE.md §6).

### Types and arithmetic
- **Do use `int` / `u32` for locals, loop counters and arguments.** The CPU is 32-bit. `u8`/`u16`
  locals save nothing and cost extra masking instructions (Tonc; pokeemerald wiki).
  Use `u8`/`u16` only for storage layout: structs, tables, hardware registers.
- **Don't use floating point.** It's software-emulated and very slow. Use fixed point (e.g. 24.8
  or 16.16) for animation and zoom factors.
- **Don't divide or modulo by non-powers of two on hot paths.** There's no divide instruction: it
  becomes a library call, and in Thumb even division by a constant may not be optimised. Use
  shifts and masks, reciprocal multiplication, lookup tables, or precomputed values.
- **Do use `unsigned` for bit manipulation and shifts.** Signed shifts and overflow are
  implementation-defined or undefined.

### Memory and code placement
- **Do run code from ROM in Thumb (`-mthumb`).** The ROM bus is 16-bit and Thumb instructions are
  16-bit.
- **Do put genuinely hot code in IWRAM as ARM** (`.iwram` section, `-marm`, called with
  `__attribute__((long_call))` or via a function pointer): 32 KB, 32-bit, zero wait states. For
  us that's things like the per-frame button-light update or the zoom matrix step, *not*
  `link_write.s` (see 3 below).
- **Do keep `const` data `const`** so it stays in ROM and doesn't get copied to RAM at boot.
- **Don't use `malloc`/heap.** Everything is static. Know your RAM budget (IWRAM 32 KB incl.
  stack and IRQ stack, EWRAM 256 KB at 16-bit and 2 wait states).
- **Watch the stack.** The IRQ stack is tiny (the crt0 sets `__sp_irq` 0x60 below the top of
  IWRAM). Handlers must not use big locals.

### Hardware access
- **Do access registers through `volatile` pointers** (libtonc's `REG_*`). Never cache a register
  read in a non-volatile variable across a wait.
- **Don't write bytes to VRAM, OAM or palette RAM.** 8-bit writes to VRAM duplicate the byte into
  the halfword, and to OAM/palette they're ignored. Use 16/32-bit writes. This includes
  `memcpy`/`memset`, which may emit byte stores: use libtonc's `memcpy16/32`, `memset16/32` or
  DMA (gbadev forum; rust-console/gba issue on OAM + memcpy).
- **Do update OAM, palettes and scroll registers in VBlank** (or HBlank), or keep shadow copies
  and copy them at VBlank. Mid-frame writes tear.
- **Do sleep with `VBlankIntrWait()` / `Halt`**, never busy-wait on `REG_VCOUNT`. Busy-waiting
  burns battery and keeps the CPU fetching from ROM (CPLD traffic).
- *(adopted)* **Don't do large DMA during play, and keep IRQ handlers tiny** (SPEC §0 rule 7).

### Project-specific rules *(adopted)*
1. `src/link_write.s` is frozen: it's a bus-faithful copy of the stock routine. Don't move it to
   IWRAM, "optimise" it, or call it from anywhere except the link layer.
2. Nothing but the link layer writes to `0x08000000–0x09FFFFFF`.
3. WAITCNT and prefetch stay at the BIOS defaults until hardware testing proves otherwise.
4. The send IRQ is the highest-priority work. Nothing may disable IRQs for more than about 100 µs
   during play.

---

## 2. Linting and static analysis

MISRA C:2012 in full is overkill for a hobby ROM, and the free checker (cppcheck's `misra.py`) is
partial, uncertified, and needs the rule texts, which can't be redistributed, so it can't run in
public CI. Proposal: **a layered, open-source set with a MISRA-inspired core**:

| Layer | Tool | What it adds | Gate |
|---|---|---|---|
| 1 | GCC warnings: `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wcast-align -Wstrict-prototypes -Wmissing-prototypes -Wundef -Wdouble-promotion -Wvla -Werror` | Catches implicit narrowing (u32→u16), accidental `double`, missing prototypes | every build |
| 2 | `gcc -fanalyzer` | Path-sensitive: NULL derefs, use-before-init, leaks | `make lint` |
| 3 | clang-tidy (`bugprone-*`, `cert-*`, `misc-*`, `readability-*` subset, `clang-analyzer-*`) | Second compiler front-end and analyser | `make lint` |
| 4 | cppcheck (`--enable=warning,style,performance,portability`, `--check-level=exhaustive`) | Different engine, catches different things | `make lint` |
| 5 *(optional, local)* | cppcheck `--addon=misra` with your own rule-text file | MISRA spot checks | manual |
| 6 | clang-format (checked-in `.clang-format`) | Consistent style, no review noise | `make format-check` |

**MISRA-inspired rules we'd adopt in practice** (enforced by the tools above where possible):
fixed-width types for storage; no implicit narrowing conversions; no recursion; no dynamic memory;
`switch` always has `default`; one exit point is *not* required; all `if/else if` chains end in
`else`; no `goto` except in documented cleanup; braces always.

**Documented deviations** (needed on bare metal):
- integer-to-pointer casts for memory-mapped registers (MISRA 11.4/11.6) via libtonc macros
- inline assembly / `.s` files (Dir 4.3) for `link_write.s` and libtonc
- libtonc and devkitPro crt0 are third-party and excluded from linting
- `-Wanalyzer-use-of-uninitialized-value` is off: a GCC 13 false positive on `memcpy` from a fully
  string-initialised array (main.c, checked by hand). Revisit on a newer GCC.

---

## 3. Testing strategy

| Level | What | How | Runs in |
|---|---|---|---|
| **Unit (host)** *(built 2026-11-14: 31 tests × 2 protocols, 100% lines / 97.7% branches of src/core)* | Pure logic: `link_pack`, protocol encoding, menu state machines, note timers, LED status rules, settings encoding (and the "no byte = 0x49" rule) | Compile the same `.c` for x86 with a register shim; **Unity** test framework (MIT, single C file) or plain asserts; gcov/gcovr coverage | seconds |
| **Protocol (emulator)** *(exists)* | Byte stream vs stock recordings | `make test`: headless mGBA, 6 scenarios | seconds |
| **Timing (emulator)** *(adopted)* | Gap ≤ 2.2 ms, press-to-send ≤ 2.2 ms during UI-heavy scenarios | Harness logs CPU-cycle timestamps per write | seconds |
| **Visual (emulator)** | Screens match approved goldens | §5 below | seconds |
| **Hardware** | What emulation can't settle: CPLD timing, flash writes, mid-session pairing, X/Y | Manual checklist (README), each result recorded with date | when the GBxCart RW arrives |

To make host unit tests possible, the code is split into **pure modules** (no registers:
`protocol_*.c`, `ui_state.c`, `status.c`) and **thin hardware modules** (`link.c`, `video.c`,
`main.c`). The pure modules are where most logic and most tests live.

---

## 4. Mutation testing (Stryker equivalent)

**Status 2026-11-14:** `tests/mutate.py` (our own, no third-party code) is built: `make mutate` and a
CI job (main / manual, ~45-60 min). History, kept honest:
- First run 39/56 (70%): survivors showed **Left was never pressed in any emulator scenario**.
- A later "155/155 (100%)" was **wrong**. Gemini's chunk-2 review found that every `src/core` mutant
  failed to *compile* its unit tests (absolute-path bug) and was counted as killed, and that stale
  object files were reused between mutants. Both are fixed: host compile failure = INVALID, fresh
  objects per mutant, timeouts, annotations scoped to named operators.
- A third bug (found by Claude after the review): `make` exits 2 for any failing recipe, so a
  failing unit test looked like a compile error and every unit kill was counted INVALID
  (196/217 = 90% honest, 0 unit kills). `unit-fast` now writes an explicit result file.
- Gemini's re-check then found a fourth: Unity exits with its *failure count*, so "2 tests failed"
  collided with the compile-error code and 37 real kills were filed INVALID. Fixed (compile = 200,
  normalised results, infra errors abort with exit 3, unmutated baseline must pass first, crashes
  count as kills). False "equivalent" annotations were replaced by tests or by the separate
  **accepted** category (reported, never counted as killed).
- **Current (2026-11-14): 248/248 valid mutants killed (100%)**: 194 unit, 18 emulator, 36 visual;
  19 invalid (don't compile for ARM), 1 accepted (`tte_set_pos` x-offset, Phase 0 text only).

Mull (host, LLVM IR) still needs the owner to install it (`/tmp/gba-mull.sh`, checksum-verified).

Options for C:
- **Mull** (LLVM-based, actively maintained): mutates at IR level, needs Clang. Fits the **host
  unit tests** (built with clang for x86). Best signal-to-noise.
- **universalmutator** (regex-based, any language, pip install): mutates source text, so it can
  mutate the **real ARM build** and score it against the emulator tests (`make test`). Slower
  (rebuild + emulator run per mutant, about 5–10 s) and noisier, but it's the only option that
  tests our protocol and timing code under the real compiler and real emulator.
- dextool mutate: LLVM-based with git-diff-only mutation, but heavier to set up. Not needed at this size.

Proposal:
1. **Mull on host unit tests**, `make mutate`, run before each phase sign-off. Target ≥ 80% killed
   on pure modules, with surviving mutants reviewed (equivalent ones annotated).
2. **universalmutator on `src/link.c` + `src/protocol.h` against `make test`.** It's small
   (about 100 lines), so a full run is minutes. This is the one that proves the regression suite
   would catch a protocol bug. (We already did one manual "mutant": the V11B build, and all 6
   scenarios caught it.)
3. Never mutate `link_write.s` (frozen by design) or third-party code.

---

**Phase 1 chunk 1 (2026-11-14):** 295/308 on the first run. Real gaps found: `view_init` could
write one byte past its struct, `view_mix` let bit 15 leak, and the text screen's forced blank was
untested; each now has a test. The send IRQ's priority mutant (`ISR_PRIO(0) -> 1`), killed in an
earlier run, survived this time: that kill was a coincidence of boot timing, since priority only
matters when two IRQs are pending at once and then costs microseconds. It is now *accepted*
(reported, never counted). Equivalent mutants (larger arrays, a sub-pixel affine shear,
off-screen map rows) are annotated in place.

## 5. Screenshot (golden image) tests

**Status 2026-11-14: built.** `tests/visual.py` + `tests/pngio.py` (no image library needed), 8 screens ×
2 protocols, `make visual` is part of `make check`, and mutation testing scores against it too. **First
run found a real bug:** the Select+Down black screen rendered *white* (layers off with backdrop 0x0000
in mGBA 0.10). Fixed the stock way (BG0 on, black palette).


mGBA is deterministic, so the same ROM plus the same key script gives the **same pixels every
run**. That allows exact comparison, no fuzzy thresholds:

- `tests/visual/*.script`: key scripts in harness syntax (e.g. boot → 30 frames → hold L+Start+Right+A → capture).
- `tests/visual/golden/*.png`: approved 240×160 frames, committed. They're our own renders, so
  they're fine to publish.
- `make visual`: render every script, compare pixel-exact. On mismatch, write
  `build/visual/<name>.diff.png` (changed pixels highlighted) and fail.
- `make visual-approve [SCREEN=pairing,dpad]`: copy changed renders over their goldens, only after a
  human has looked at them (`build/visual/<ver>/*.actual.png`).
- CI pins `libmgba-dev=0.10.2+dfsg-1.1build3` (Ubuntu 24.04). A different emulator version is a
  legitimate reason to re-approve.
- Animated screens (splash, zoom) are captured at fixed frame numbers, so they're deterministic too.
- Bonus: the same captures can be laid next to the design mockups for review (not a pass/fail check).

---

## 6. Asset pipeline (graphics and font)

Sources are committed; everything the ROM uses is generated at build time into `build/gen/`
(never committed, rebuilt when the source or the tool changes).

| Source | Tool | Output |
|---|---|---|
| `assets/controller.svg` (itself generated by `tools/gen_controller_svg.py` from `assets/controller_cad.json`, outlines traced from the owner's CAD by `tools/cad_trace.py`) | `tools/svg_to_gba.py` | `ctrl.{c,h}`: 8bpp tiles, 32x32 affine map, palette, `enum ctrl_part` |
| `assets/fonts/PressStart2P-Regular.ttf` | `tools/ttf_to_tfont.py` | `font_ps2p.c`: libtonc `TFont`, 8x8 1bpp, ASCII 32..127 |

- **One palette entry per SVG part**, hard edges (a pixel is a part's when it is at least half
  covered; later parts paint over earlier ones). Lighting a button = one palette write in VBlank,
  no tile or map traffic, so the UI costs nothing on the input path.
- **Pixel shading as palette entries** (the approved mockups' rules, `data-shade` attributes in the
  SVG): raised parts get `<part>-hi` on top/left edge pixels and `<part>-lo` on the bottom edge
  (the D-pad's arms shade as one group); the body and trims get a dark rim; the lens a light top
  edge; the LED a 2-ring glow into the body. Lighting a button = writing its 3 entries; LED status
  = LED + 2 glow entries. 53 entries in all.
- **Affine background** (for the Settings zoom): map entries are bytes, so at most 256 tiles, no
  flips. The tool dedupes identical tiles and fails the build above 256 (232 px wide, shaded: 193 tiles).
- **Font must sit on its pixel grid:** the converter fails if any rendered pixel is grey, so no
  anti-aliasing choice is ever made silently.
- Both tools are deterministic and tested (`tests/test_tools.py`, `make tools-test`, in `make check`).
  Needs `python3-pil` and `librsvg2-bin`.

**Phase 1 chunk 2 (2026-11-14):** 434/466 on the first run. Real gaps: `lcd_pixel` could write a row
past the canvas, the bar's tile-row maths only worked for today's LCD height, and timers were never
checked straight after init. All fixed with tests; `lcd.c` then 134/134. Two map-clear mutants are
*accepted* (invisible with today's notes and no sprites, stated in place).

**Phase 1 chunk 3 (2026-11-15):** after the review fixes, 329/330 on the chunk's files; the survivor
(an empty bar redrawn on a blank title page) is annotated equivalent.

**Phase 1 chunk 4 (2026-11-15):** 517/612 on the first run, and most "survivors" were a runner gap:
for a core file only its own test ran, so canvas.c's glyph code (tested through test_lcd.c) looked
untested; `ALSO_TESTED_BY` in tests/mutate.py fixes that. Real gaps then got tests (boot PLAY, 3-digit
values, a note after Settings, rect edges). Two canvas_rect survivors exposed an undefined shift
(> 31 bits) that x86 happens to wrap to the right answer, so host tests could never see it; the code
now takes the offsets from x0 & 7 / x1 & 7. Final: 257/257 on those files. *Accepted:* swapping the
assumed channel/address/packets (the defaults are all 1).

**Phase 1 chunk 5 (2026-11-15):** a process slip first: a blanket `make visual-approve` accepted a
regression (boot frame 8 white again); Gemini caught it. Approvals now always name the screens looked
at (`make visual-approve SCREEN=...`). Mutation then found guard words that couldn't see an overrun
writing zeros (now sentinels), untested nibble patterns, and a test ink equal to the sentinel's
nibble that masked an overrun. 591/604, then 173/173 on the canvas. *Accepted:* the VBlank key latch.

**Colour-only captures.** A capture named `*_colours` (e.g. `pairing__frame7_colours`) is compared by
the set of colours it contains, not pixel by pixel. It's for boot transition frames, where the
scanline at which startup finishes moves with code size, but what matters is that no garbage
colour ever shows.

## Sources
- Tonc / libtonc notes (32-bit types, general GBA programming):
  https://www.github.com/gbadev-org/libtonc/blob/master/libtonc.txt
- "Why you should (almost) always be using 32-bit variables" (paraphrases Tonc §3.3):
  https://github-wiki-see.page/m/pret/pokeemerald/wiki/Why-You-Should-(Almost)-Always-Be-Using-32%E2%80%90Bit-Variables-in-Your-Code
- BlocksDS "optimizing code" (Thumb division by constants, ARM code placement):
  https://blocksds.skylyrac.net/tutorial/advanced/optimizing_code/
- gbadoc, memory-mapped registers and VBlank sync: https://gbadev-org.github.io/gbadoc/registers.html
- OAM + memcpy byte-write pitfall: https://gh.r-mj.com/rust-console/gba/issues/213
- GBA graphics, volatile VRAM pointers: https://ianfinlayson.net/gba/02-graphics.html
- Mull: https://arxiv.org/pdf/1908.01540, https://mull.readthedocs.io/en/0.9.0/Features.html
- universalmutator: https://www.cefns.nau.edu/~adg326/icse18t.pdf
- cppcheck MISRA addon (rule texts not distributable): https://sources.debian.org/src/cppcheck/2.20.0-2/addons/README.md
- clang-tidy + cppcheck as the open-source layered approach: https://notebook-dg9.pages.dev/kb/embedded/misra-cpp/tooling-compliance/

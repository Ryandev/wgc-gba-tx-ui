# Wireless GBA Controller: hardware and protocol notes

What I could find about the insideGadgets **Wireless Gameboy Controller** (GBA TX cart and USB
receiver), plus what I reverse-engineered from the stock ROM. I wrote this on 2026-11-14 as
groundwork for a new, nicer cart UI. Read this before you change the ROM.

How confident each section is:
- **[doc]**: stated by insideGadgets (shop page, manual or build blog).
- **[RE]**: recovered by disassembling the stock ROMs in `reference/stock-rom/`. These are facts
  about what the GBA sends. What the ATtiny does with it is inferred.
- **[inferred]**: my reading of the evidence. Not verified on hardware yet.

---

## 1. System overview

```
 ┌──────────── GBA / GBA SP / Micro / DS ─────────────┐
 │  ROM (UI + key scanner) runs from cart flash       │
 │  every ~2 ms: strh <6/7-bit value> -> 0x08000000   │
 └───────────────┬────────────────────────────────────┘
                 │ cart bus (AD0-15 muxed, A16-23, /CS, /RD, /WR)
 ┌───────────────▼──────────── GBA TX cart ───────────┐
 │  MAX V CPLD: latches address on /CS, increments it │
 │  on /RD (so flash looks like a GBA ROM), captures  │
 │  data bits on /WR and raises a TRIG line           │
 │  ATtiny48: reads the bits on TRIG, rebuilds bytes, │
 │  stores RF config in EEPROM, drives the nRF24      │
 │  nRF24L01+: 2.4 GHz, no ACK, no retries, no crypto │
 └───────────────┬────────────────────────────────────┘
                 │ 2.4 GHz (channel 1-125, 1-byte payloads)
 ┌───────────────▼────────── USB receiver ────────────┐
 │  ATtiny841 + nRF24L01+ + V-USB (bit-banged USB)    │
 │  USB HID joystick, polled every 8 ms               │
 └───────────────┬────────────────────────────────────┘
                 ▼
          Raspberry Pi 4 / RetroArch (giant GBA)
```

The UI on the GBA screen is an ordinary GBA program stored in the cart's flash chip. **To change
the UI, you flash a new ROM** with a GBxCart RW or similar. The cart's microcontroller (ATtiny)
firmware stays as it is. insideGadgets say so themselves: *"You will require a flasher like GBxCart
RW to update"* [doc].

---

## 2. GBA TX cart, board "GBA WGC TX v1.0"

Read from `reference/photos/Connection_points_to_program.jpg` (an older board; mine may be a later
revision, see §6).

| Ref | Part | Role |
|---|---|---|
| U3 | **Macronix MX29LV320CBTC-70G**: 32 Mbit (4 MB) parallel NOR flash, bottom boot, 70 ns | Holds the GBA ROM. Uses JEDEC `0x555/0x2AA` command unlock. |
| U1 | **Altera MAX V 5M40Z** (TQFP-64, 40 LEs) [doc, blog Part 6] | Turns the GBA's multiplexed bus into flash addresses and captures writes |
| U6 | 50 MHz oscillator [doc] | CPLD clock. Inputs go through 2 flip-flops to sync them (adds about 40 ns of latency). |
| U5 | **Atmel ATtiny48** (QFN-32), 8 MHz internal [doc] | Reads the CPLD outputs, holds the RF config in EEPROM, talks SPI to the nRF |
| U4 | Regulator (on the 1.8 V test pad) [inferred] | 1.8 V core supply for the MAX V |
| module | **nRF24L01+** with PCB antenna | 2.4 GHz radio |
| D1 | LED | Lights when packets are sent |
| pads | `TCK TMS TDI TDO 1.8v GND` | CPLD JTAG |
| pads | `RST SCK MOSI MISO 3.3V GND` | ATtiny ISP. avrdude/USBasp at 3.3 V. The rumble mod `.bat` files use `-p attiny441` as the part ID. |

Later variants: the rumble-mod bundle includes both `gba_tx_attiny48_*.hex` **and**
`gba_tx_attiny88_*.hex`, so some boards use an ATtiny88 [doc]. The shop also says ROM **v1.1b is
"only for carts purchased after 2023"** and v1.1a is "only for carts purchased before 2022" [doc].
That means a hardware revision with a **different, incompatible bus protocol** (§4).

### How the CPLD works [doc, Part 6]
- When /CS goes low it latches A0-15. Each rising /RD increments that address (GBA sequential
  burst). A16-23 go straight to the flash.
- When /WR goes low it captures **7 bits** of data (`inputAddress[6:0]`, which is AD0-6) and raises
  `outputReady`. The ATtiny polls that pin (`CPLD_TRIG`). Only 7 bits pass because that was all
  that fitted in the 40 LEs of the 5M40Z.
- The GBA doesn't use /WR internally, so any write to cart space reaches the ATtiny.
- Known problem: some GBAs won't show the boot logo when the cart is pushed fully home. Pull it
  out a little [doc].

### Range and power [doc]
- At least 4 m line of sight indoors with the GBA cart (8 m with the GB cart). There are dead zones
  depending on orientation, and fingers near the module make it worse. Raising Pcks/Refresh helps a
  little.
- You may hear a buzz from the speaker. That's the RF bursts and it's normal.

---

### Save storage [inferred from board photo]
There's no GBA-visible save chip (no SRAM/FRAM/EEPROM). The only writable storage is the ROM flash
itself (4 MB, about 100 KB used) and the ATtiny's internal EEPROM, which the GBA can't read. See
SPEC F for the flash-sector settings plan.

## 3. USB receiver

| | |
|---|---|
| MCU | **ATtiny841** (switched from ATtiny84 in fw v1.1 after the DO/DI swap) [doc] |
| USB | **V-USB**: bit-banged USB 1.1 low-speed HID [doc] |
| VID:PID | `0x16C0:0x27DC`, the shared V-USB "USB Joystick" ID [doc, 2018 blog; check with `lsusb`] |
| Report | X/Y axes **-127..127** (fw ≥ 1.3; older firmware sent 0..2 and broke some apps) plus buttons |
| Poll | **8 ms** (125 Hz), tested on Win7 and Ubuntu 18.04 [doc] |
| Pairing | On power-up it listens about 0.5 s on a fixed address for a config packet, then uses its EEPROM channel and address. Newer firmware scans for a GB or GBA TX and blinks about 1 Hz while searching [doc]. |
| Options | Hold **Down** on the GBA while plugging the RX in → D-pad mode instead of axes (fw 1.6) [doc] |
| Firmware | Latest public build is `WGC_RX_Firmware_31_July_2023.zip`, flashed over the 6-pin ISP with a 3.3 V USBasp [doc] |
| LED | Solid = receiving. Flicker or dim = packet loss, so change channel or Pcks [doc] |

Not supported: Switch, Analogue Pocket Dock. Only one TX can be on at a time. Keep carts at least
3 channels apart [doc].

**Latency, compared with the giant-GBA target of < 15 ms (`Notes/Diary.md`):** cart scan about 2 ms,
plus RF (sub-ms), plus USB poll ≤ 8 ms, gives roughly **≤ 10 ms worst case** before RetroArch sees
the input. That fits [inferred].

---

## 4. Cart bus protocol (stock ROM to ATtiny) [RE]

Every message is a 16-bit store, `strh value, [0x08000000]`, followed by a short busy-wait. The
stock ROM counts a `volatile u8` from 0 to 50, which gives the 8 MHz ATtiny time to poll TRIG.

### 4.1 Message constants by ROM version

| Message | v1.1a (pre-2022 carts) | **v1.1b** (2023+ carts) |
|---|---|---|
| Boot sync, written once at start | `0x49` | `0x29` |
| "GB mode" (A held at boot) | `0x63, 0x5A` | `0x23, 0x1A` |
| Config header | `0x6D, 0x52` | `0x2D, 0x12` |
| Key word split | `hi = W >> 7`, `lo = W & 0x7F` | `hi = W >> 6`, `lo = W & 0x3F` |
| L+R+A → X, L+R+B → Y | no | **yes** |

Every v1.1b constant fits in 6 bits. That suggests the newer cart hardware only has a **6-bit data
path**, or uses bit 6 for something else [inferred]. **A custom ROM must use the constants that
match your cart.** Check the version string on the stock boot screen first (§6).

### 4.2 Key word `W` (12 bits)

| bit | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| key | A | B | Right | Left | Up | Down | Select | Start | L | R | X* | Y* |

\* v1.1b only. When L **and** R are both held, A also sets bit 10 and B also sets bit 11. The A, B,
L and R bits stay set as well.

The low byte matches the original Game Boy TX cart exactly (A = bit 0 … Start = bit 7). Without
GB mode the ATtiny sends the extra bits as a second RF packet [doc, Part 6].

`W` comes from `REG_KEYINPUT (0x04000130) ^ 0x3FF`, remapped bit by bit. The GBA's native order
is A, B, Sel, Start, Right, Left, Up, Down, R, L, which is **not** the same order.

### 4.3 Normal-mode loop

```c
// once at boot
write(SYNC);               delay();
if (A held)  { write(GB1); delay(); write(GB2); delay(); }   // "GB mode"
// each tick (timer 3 IRQ wakes the CPU)
W = remap(~REG_KEYINPUT);
write(W >> 6); delay();    // v1.1b
write(W & 0x3F); delay();
SWI 2 (Halt) until the next timer-3 IRQ
```

Timer 3 runs at 16.78 MHz / 256 with `TM3CNT_H = 0xC2`:
- With the screen off (Select or Select+Down held at boot): reload `0xFF82`, which is 126 ticks, or
  **1.92 ms**.
- With the screen on: reload `0xFFDC`, which is 36 ticks, or 0.55 ms. The libgba `printf` on every
  pass makes the real period much longer. The author tuned it by watching writes on a scope
  [doc, Part 6].

**Implication for a new UI:** run the key send from the **timer-3 IRQ** so the 2 ms cadence holds
whatever the renderer is doing. Do the drawing in the main loop or on VBlank.

### 4.4 Config mode (Start held at boot)

Menu state lives in IWRAM `0x030001FC`: `{redraw, channel, address, pcks, row, screenOn}`. The
defaults in `.data` are all `1`. **The ROM can't read the cart's current settings.** It always
starts at 1/1/1.

| Field | Range | Notes |
|---|---|---|
| RF Channel | 1-125 | nRF channel, 2400 + ch MHz |
| RF Address | 1-254 | Last byte of the 5-byte pipe address. The base names in the 2018 source are `"wgb01"` and `"wgbtx"`. |
| Pcks/Refresh | 1-4 | How many times the ATtiny repeats each packet. The GBA side doesn't use it. |

Controls: Up/Down picks a row, Left/Right changes the value, with about a 50,000-loop debounce. When
you press **A**, the ROM sends this once and shows "Transmitting...":

```
write(CFG1); delay();  write(CFG2); delay();
write(channel); delay();
write(address); delay();
write(((address >> 1) & 0x40) | pcks); delay();   // address bit 7 travels in bit 6
```

After that the ATtiny writes the values to EEPROM and keeps sending them on a fixed pairing address
until power-off. Plug the receiver in (it blinks twice), then power-cycle the GBA [doc].

> Warning [inferred]: on v1.1b, `address` (up to 254) and the bit-6 flag don't fit in 6 bits. Either
> the newer hardware really passes 7 bits, or addresses above 63 don't pair properly. The same
> applies to **channels 64–125** (bit 6 lost). **A custom UI should limit channel and address to
> 1-63 until this is tested.** (Our v1.1b build does: `PROTO_MAX_CH/ADDR`.)

### 4.5 Boot options [doc + RE]

| Held at boot | Effect |
|---|---|
| Start | Config menu |
| Select | Screen off (grey palette, no printing, 1.92 ms timer). Saves power. |
| Select + Down | Black screen, for backlit and IPS mods |
| B | Shows key letters (`L R U D A B S T L R X Y`) instead of `.` |
| A | GB mode: no L/R, for old receivers |

---

## 5. Stock ROM internals [RE]

| | |
|---|---|
| Toolchain | devkitARM + **libgba** (console `printf` and `\x1b[row;colH` escapes, newlib) |
| Header title | `IG.GBA.TX` (v1.1b), blank (v1.1a), maker `01` |
| Size | about 78 KB (most of it newlib `printf`) |
| `main` | `0x08002258` (v1.1b), `0x080021F4` (v1.1a) |
| Key routines (v1.1b) | config menu `0x08000230`, key send `0x0800056C`, irqInit `0x08000894` |
| Strings | `0x0800DDB0`–`0x0800DEEC` ("insideGadgets", "Wireless GBA Controller v1.1b", menu text) |
| WAITCNT | **Never written**, so it stays at the BIOS default (4/2 waitstates, no prefetch) |
| Video | Mode 0, BG0 console. Palette 0 = `0x2108` (dark grey), text = `0x7FFF`. |

`tools/disasm_thumb.py <rom> <start> <end> [t|a]` uses Capstone
(`pip install --target ... capstone`) and resolves PC-relative literals.

---

## 6. Rules for a custom UI ROM

1. **Find out your cart revision first.** Boot it without holding anything and read line 2:
   `Wireless GBA Controller v1.1a` or `v1.1b`. Dump the ROM with GBxCart RW before flashing
   anything, and check it against `reference/stock-rom/SHA256SUMS`.
2. **Use the protocol exactly as in §4**, with 16-bit `volatile u16*` stores to `0x08000000` and
   the inter-write delay. Don't write anywhere else in `0x08000000–0x09FFFFFF`. Every cart write
   reaches the ATtiny, and the NOR flash would see a `0xAA→0x555, 0x55→0x2AA` sequence as a command.
3. **Leave WAITCNT at the BIOS default** or test carefully. All ROM reads go through a CPLD that
   synchronises /CS and /RD at 50 MHz, so faster waitstates or prefetch may break code fetch.
   Copy hot code and data to IWRAM/EWRAM instead.
4. **Send keys from the timer-3 IRQ** at about 2 ms. Keep IRQ work tiny and render on VBlank.
5. Keep the boot-key options (Start/Select/B/A/Select+Down) so the manual still works.
6. Size isn't a limit (4 MB flash). Without newlib `printf`, a tile and sprite UI will probably be
   *smaller* than the stock ROM.
7. Debug in mGBA. Writes to `0x08000000` are harmless there, and you can log them with a Lua or
   scripting hook to check the byte stream against §4.

---

## Sources
- Shop page (archived 2026-06-03): https://shop.insidegadgets.com/product/wireless-gameboy-controller/
- Manual v1.0 rev 2: `reference/Wireless-Gameboy-Controller-v1.0-Manual-Rev2.pdf`
- Build blog Part 1: https://www.insidegadgets.com/2018/09/30/building-a-wireless-gameboy-controller-acts-as-a-usb-hid-joystick/
- Part 2 (config, CGB timing): https://www.insidegadgets.com/2018/10/26/building-a-wireless-gameboy-controller-part-2-adding-rf-channel-address-configuration-and-more-testing/
- Part 3 (cart↔cart, 2-player Pong): https://www.insidegadgets.com/2018/11/05/building-a-wireless-gameboy-controller-part-3-communicating-between-carts-for-2-player-games/
- Part 6 (GBA TX cart, CPLD Verilog): https://www.insidegadgets.com/2019/01/29/building-a-wireless-gameboy-controller-part-6-making-the-gba-tx-cart/
- Stock ROMs v1.1a/v1.1b and the rumble-mod bundle: shop download links via the Wayback Machine (`reference/`)
- License: insideGadgets material is CC BY-NC (manual CC BY-NC 3.0, 2018 source CC BY-NC-SA). The
  ROMs are kept here only as a personal backup and reference.

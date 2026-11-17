# Flashing and updates: what goes where

The system holds **three separate programs**. Each one is updated in a different way.

| # | Program | Lives in | Update tool | Do we need to touch it? |
|---|---|---|---|---|
| 1 | **GBA ROM**: the UI you see on screen, plus the key scanner | 4 MB flash chip on the cart | **Cart flasher** (GBxCart RW + FlashGBX), no soldering | **Yes.** This is the whole UI project. |
| 2 | Cart ATtiny firmware: bus to radio bridge | ATtiny48 inside the cart | USBasp (3.3 V) + avrdude on solder pads `RST SCK MOSI MISO 3.3V GND` | No |
| 3 | USB receiver firmware | ATtiny841 in the receiver | USBasp (3.3 V) + avrdude on its 6-pin ISP header (cut the heat shrink, sometimes solder) | No (optional, for newer features) |

Most owners never update anything. The carts and receivers ship pre-paired and working. The
firmware updates on the shop page are optional extras for hobbyists. Updates 2 and 3 need an
AVR programmer and some fiddling with pads. Update 1 is plug-and-play once you have a flasher.

## 1. Reflashing the GBA ROM (the path we need)

### Which ROM file to use
Every CI run on GitHub Actions builds and validates both cart revisions and uploads them in the **`roms`**
artifact (`make dist` stages the same bundle locally in `build/dist/`):
- **`wgc-gba-tx-ui-v1.1a.gba`** (`build/wgc-gba-tx-ui.gba`): for **v1.1a** carts (pre-2023; stock screen shows `v1.1a`).
- **`wgc-gba-tx-ui-v1.1b.gba`** (`build/v11b/wgc-gba-tx-ui.gba`): for **v1.1b** carts (2023+; stock screen shows `v1.1b`).

### Hardware & software
- **Hardware:** an insideGadgets **GBxCart RW** (v1.4 / v1.4 Pro or GBxCart RW Mini; v1.4 has the best
  flash-cart support). It's a USB box with a GBA cart slot and runs on Windows, Linux and macOS.
- **Software:** **FlashGBX** (free, open source: https://github.com/lesserkuma/FlashGBX). It also keeps
  the GBxCart's own firmware up to date.

### Steps
1. Plug the GBxCart RW into the PC by USB and set it to **GBA / 3.3 V** mode.
2. Insert the wireless TX cart into the GBxCart slot. Use the slot, not the GBA.
3. Open FlashGBX and pick **Game Boy Advance**. It reads the header (title blank on stock v1.1a).
4. **Back up first:** *Backup ROM* → save as `my_cart_backup.gba` and keep it safe. A dump can come
   out padded to a power of two (e.g. 128 KB); the unpadded stock ROM SHA-256 hashes are:
   - `v1.1a` (first 78,920 bytes): `d1a7151f42af7873fe21e46eed1624a36819da7a24b7347c7283a8561ca50cb8`
   - `v1.1b` (first 79,016 bytes): `e4413653c8f8617549d42223681e1cf9d661ff856349aaa1affd6ae2385e17d3`
5. *Write ROM* → choose `wgc-gba-tx-ui-v1.1a.gba` (or `-v1.1b.gba` to match your cart). The flash chip
   is a Macronix **MX29LV320** (JEDEC/AMD command set). Try auto-detect first. If that fails, choose a
   generic flash-cart type that uses the `0x555/0x2AA` command set.
6. Put the cart back in the GBA and boot it. If anything goes wrong, write `my_cart_backup.gba` back the
   same way. You can't permanently break the cart this way: the ATtiny and its RF pairing (stored in
   ATtiny EEPROM) aren't touched.

Not yet verified for this particular cart: whether FlashGBX auto-detects it. insideGadgets
officially say a GBxCart RW can update it (shop page: *"You will require a flasher like GBxCart RW
to update"*).

## Without a flasher (where we are now)

- **Develop and test in an emulator (mGBA).** That covers nearly all the UI work: graphics,
  menus, animations, and checking the exact bytes written to `0x08000000` against `HARDWARE.md` §4.
  The only thing an emulator can't show is the real radio link.
- Self-flashing (a ROM rewriting its own flash from RAM) is possible in theory. It isn't a real
  option here: the stock ROM can't do it, and loading a program over the link cable needs other
  hardware. Not worth the risk.
- So the GBxCart RW is the one purchase needed, and only when there's a ROM worth putting on the cart.

# Licences

| What | Licence |
|---|---|
| Code in this repo (`src/`, `tests/`, `tools/`, `scripts/`, `Makefile`) | MIT (see `LICENSE`) |
| Documentation (`*.md`) | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) |
| libtonc, fetched at build time (`scripts/fetch_deps.sh`) | MIT, © J Vijn |
| `gba_crt0.s` from devkitPro/devkitarm-crtls, fetched at build time and linked into the ROM | MPL-2.0 (file-level; used unmodified). Source: github.com/devkitPro/devkitarm-crtls @ 1c0c102 |
| `ld/gba_cart.ld`, adapted from devkitarm-crtls (two small changes, marked in the file) | Public domain (per its header) |
| Press Start 2P font (`assets/fonts/PressStart2P-Regular.ttf`, unmodified) and the ROM's glyphs rendered from it | [SIL Open Font License 1.1](assets/fonts/OFL.txt), © 2012 The Press Start 2P Project Authors (cody@zone38.net), Reserved Font Name "Press Start 2P". Source: github.com/google/fonts `ofl/pressstart2p` |
| `gbafix` from devkitPro/gba-tools, fetched and built at build time, not part of the ROM | GPL-3.0 |

`assets/controller.svg` and `assets/controller_cad.json` are generated from the project owner's own
1:1 CAD model of the console (front-view outlines of its parts, `tools/cad_trace.py`); no
third-party drawing is used and no CAD model is included in the repo. Owner to confirm: the CAD's
part placement refers to a reference `GBA.STEP`; if that model came from a third party, record its
source and licence here. The drawing carries no lettering or logos.

**About the font:** the OFL lets us embed the font in the ROM and redistribute it, provided the
licence and copyright notice go with every copy and the font is not sold on its own. So the TTF
sits next to `assets/fonts/OFL.txt`, CI's ROM downloads include `OFL.txt` and this file, and
releases must too. The ROM's glyph table (`tools/ttf_to_tfont.py`: 96 ASCII glyphs converted to a
1bpp bitmap) counts as an OFL **Modified Version** (OFL §"Definitions": changing formats or porting
to a new environment). It therefore stays under the OFL and must not use the Reserved Font Name:
it's called `ps2pFont` in code and credited as "glyphs rendered from Press Start 2P". The About
screen carries the credit (SPEC.md).

Unofficial project, not affiliated with insideGadgets or Nintendo. "Game Boy Advance" is a
Nintendo trademark, used only to describe what the controller is. The cart protocol notes are
an independent write-up from disassembling the stock ROMs for interoperability. No insideGadgets
files are included in this repo; links to their downloads are in `HARDWARE.md`.

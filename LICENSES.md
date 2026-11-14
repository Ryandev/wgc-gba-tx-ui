# Licences

| What | Licence |
|---|---|
| Code in this repo (`src/`, `tests/`, `tools/`, `scripts/`, `Makefile`) | MIT (see `LICENSE`) |
| Documentation (`*.md`) | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) |
| libtonc, fetched at build time (`scripts/fetch_deps.sh`) | MIT, © J Vijn |
| `gba_crt0.s` from devkitPro/devkitarm-crtls, fetched at build time and linked into the ROM | MPL-2.0 (file-level; used unmodified). Source: github.com/devkitPro/devkitarm-crtls @ 1c0c102 |
| `ld/gba_cart.ld`, adapted from devkitarm-crtls (one-line change, marked in the file) | Public domain (per its header) |
| `gbafix` from devkitPro/gba-tools, fetched and built at build time, not part of the ROM | GPL-3.0 |

Unofficial project, not affiliated with insideGadgets or Nintendo. The cart protocol notes are
an independent write-up from disassembling the stock ROMs for interoperability. No insideGadgets
files are included in this repo; links to their downloads are in `HARDWARE.md`.

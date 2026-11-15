#!/usr/bin/env python3
"""Screenshots of the ROM in the headless emulator, as PNGs and a labelled contact sheet.

    python3 tools/shots.py OUTDIR STEP... [--rom build/wgc-gba-tx-ui.gba] [--scale 2]
                                        [--mockup mockups/NAME.png] [--dirty]

STEPs are the harness's (tools/emu_harness.c): k:KEYS (A B s=Select S=Start > < ^ v r=R l=L, '-' none),
f:N (run N frames), p:NAME (capture). Keys before the first f: are held at boot.

Writes OUTDIR/NAME.png for each capture (240x160 x --scale), OUTDIR/sheet.png (all captures in order,
labelled) and, with --mockup, OUTDIR/vs_mockup.png (the mockup next to each capture at the same size).
Needs the ROM and build/emu_harness (`make build/emu_harness`) and Pillow. See the screenshots skill
(.claude/skills/screenshots/SKILL.md) for recipes.
"""
import argparse
import os
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def read_ppm(path):
    with open(path, 'rb') as f:
        data = f.read()
    # P6\n240 160\n255\n<rgb>
    parts = data.split(b'\n', 3)
    w, h = map(int, parts[1].split())
    return Image.frombytes('RGB', (w, h), parts[3])


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('outdir')
    ap.add_argument('steps', nargs='+')
    ap.add_argument('--rom', default=os.path.join(ROOT, 'build', 'wgc-gba-tx-ui.gba'))
    ap.add_argument('--harness', default=os.path.join(ROOT, 'build', 'emu_harness'))
    ap.add_argument('--scale', type=int, default=2)
    ap.add_argument('--mockup')
    ap.add_argument('--dirty', action='store_true', help='start with garbage VRAM, as real hardware')
    a = ap.parse_args(argv)

    names = [s[2:] for s in a.steps if s.startswith('p:')]
    if not names:
        sys.exit('shots: no p:NAME step, nothing to capture')
    for path in (a.rom, a.harness):
        if not os.path.exists(path):
            sys.exit(f'shots: {path} missing (make, make build/emu_harness)')
    os.makedirs(a.outdir, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        cmd = [a.harness, a.rom, tmp] + (['dirty'] if a.dirty else []) + a.steps
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode:
            sys.exit(f'shots: harness failed: {r.stderr.strip()}')
        shots = [(n, read_ppm(os.path.join(tmp, n + '.ppm'))) for n in names]

    s = a.scale
    for n, im in shots:
        im.resize((im.width * s, im.height * s), Image.NEAREST).save(os.path.join(a.outdir, n + '.png'))

    cols = min(4, len(shots))
    rows = -(-len(shots) // cols)
    w, h, label = 240 * s, 160 * s, 14
    sheet = Image.new('RGB', (cols * (w + 8), rows * (h + label + 8)), (255, 255, 255))
    d = ImageDraw.Draw(sheet)
    for i, (n, im) in enumerate(shots):
        x, y = (i % cols) * (w + 8), (i // cols) * (h + label + 8)
        sheet.paste(im.resize((w, h), Image.NEAREST), (x, y + label))
        d.text((x + 2, y), n, fill=(0, 0, 0))
    sheet.save(os.path.join(a.outdir, 'sheet.png'))

    if a.mockup:
        mock = Image.open(a.mockup).convert('RGB').resize((w, h), Image.NEAREST)
        vs = Image.new('RGB', (2 * w + 8, len(shots) * (h + label + 8)), (255, 255, 255))
        d = ImageDraw.Draw(vs)
        for i, (n, im) in enumerate(shots):
            y = i * (h + label + 8)
            d.text((2, y), os.path.basename(a.mockup), fill=(0, 0, 0))
            d.text((w + 10, y), n, fill=(0, 0, 0))
            vs.paste(mock, (0, y + label))
            vs.paste(im.resize((w, h), Image.NEAREST), (w + 8, y + label))
        vs.save(os.path.join(a.outdir, 'vs_mockup.png'))
    print(f'shots: {len(shots)} capture(s) -> {os.path.relpath(a.outdir)}/ (sheet.png'
          + (', vs_mockup.png' if a.mockup else '') + ')')
    return 0


if __name__ == '__main__':
    sys.exit(main())

#!/usr/bin/env python3
"""Make the README's pictures: selected golden screenshots at 3x and an animated demo.

    python3 tools/readme_images.py        (needs make, make build/emu_harness, Pillow)

The stills come straight from tests/visual/golden/v1.1a/ (the pixel-exact, tested screens), scaled 3x
with nearest-neighbour so the pixel art stays crisp; the demo GIF is recorded from the ROM in the
headless emulator. Output: docs/images/. Re-run after goldens change.
"""
import os
import subprocess
import sys
import tempfile

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GOLDEN = os.path.join(ROOT, 'tests', 'visual', 'golden', 'v1.1a')
OUT = os.path.join(ROOT, 'docs', 'images')
SCALE = 3

# golden name -> README image name
STILLS = {
    'boot__splash_title': 'splash.png',
    'chords__r_up_left_start': 'buttons.png',
    'gb_mode__l_amber': 'gb_mode.png',
    'settings__zoom_mid': 'zoom.png',
    'settings__home': 'settings_home.png',
    'settings__display': 'settings_display.png',
    'settings_boot__pairing': 'settings_pairing.png',
    'settings__about': 'settings_about.png',
    'idle__dim': 'idle_dim.png',
    'boot__splash_tip': 'splash_tip.png',
}

# Demo: (steps between captures, frames) - the splash, presses, a GB note, Settings and back.
EVERY = 3  # capture every 3rd frame (20 fps)


def demo_steps():
    """Harness steps for the demo and the number of captures."""
    script = [  # (keys, frames)
        ('-', 192),            # splash: fade, power-on, title, tip, main
        ('A', 12), ('-', 10), ('^', 12), ('>', 12), ('-', 10), ('lr', 15), ('-', 10), ('B<', 15), ('-', 25),
        ('lrs', 64), ('-', 30),   # hold L+R+Select: zoom into the LCD, Settings fades in
        ('v', 4), ('-', 8), ('v', 4), ('-', 8), ('A', 4), ('-', 40),  # DISPLAY
        ('B', 4), ('-', 8), ('v', 4), ('-', 8), ('v', 4), ('-', 8), ('A', 4), ('-', 40),  # ABOUT
        ('B', 4), ('-', 8), ('B', 4), ('-', 40),  # back, and out to the controller
        ('S', 10), ('-', 20),
    ]
    steps, n = ['f:8'], 0  # skip the boot frames (white, as stock: the display is blanked until main)
    for keys, frames in script:
        steps.append('k:' + keys)
        for _ in range(frames // EVERY):
            steps += [f'f:{EVERY}', f'p:f{n:04d}']
            n += 1
    return steps, n


def read_ppm(path):
    with open(path, 'rb') as f:
        parts = f.read().split(b'\n', 3)
    w, h = map(int, parts[1].split())
    return Image.frombytes('RGB', (w, h), parts[3])


def main():
    os.makedirs(OUT, exist_ok=True)
    for golden, name in STILLS.items():
        im = Image.open(os.path.join(GOLDEN, golden + '.png')).convert('RGB')
        im.resize((im.width * SCALE, im.height * SCALE), Image.NEAREST).save(os.path.join(OUT, name))

    rom = os.path.join(ROOT, 'build', 'wgc-gba-tx-ui.gba')
    harness = os.path.join(ROOT, 'build', 'emu_harness')
    steps, n = demo_steps()
    with tempfile.TemporaryDirectory() as tmp:
        r = subprocess.run([harness, rom, tmp] + steps, capture_output=True, text=True)
        if r.returncode:
            sys.exit(f'readme_images: harness failed: {r.stderr.strip()}')
        frames = [read_ppm(os.path.join(tmp, f'f{i:04d}.ppm')) for i in range(n)]
    big = [f.resize((f.width * 2, f.height * 2), Image.NEAREST).quantize(colors=64, dither=Image.NONE)
           for f in frames]
    big[0].save(os.path.join(OUT, 'demo.gif'), save_all=True, append_images=big[1:],
                duration=1000 * EVERY // 60, loop=0, optimize=True)
    print(f'readme_images: {len(STILLS)} stills + demo.gif ({n} frames) -> docs/images/')


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Golden-image (screenshot) tests: render screens in headless mGBA and compare pixel-exact.

    python3 tests/visual.py <emu_harness> <rom.gba> <v1.1a|v1.1b> [--approve]

mGBA is deterministic: the same ROM + key script gives identical pixels every run, so the check is
exact (no fuzzy threshold). On mismatch it writes build/visual/<ver>/<name>.actual.png and
<name>.diff.png (changed pixels in red over a dimmed copy) and fails. --approve copies the
renders over the goldens. Only do that after looking at them.
Goldens: tests/visual/golden/<ver>/<name>.png (our own renders, fine to publish).
"""
import os, shutil, subprocess, sys, tempfile
from pngio import read_png, write_png, read_ppm

HERE = os.path.dirname(os.path.abspath(__file__))

# name -> harness steps ending in p:shot (the frame captured)
SCREENS = {
    'boot_idle':      ['f:30', 'p:shot'],
    'keys_lra':       ['f:30', 'k:lrA', 'f:3', 'p:shot'],            # KEYS 301 (v1.1a) / F03 (v1.1b, X)
    'keys_each_dpad': ['f:30', 'k:^', 'f:2', 'k:v', 'f:2', 'k:<', 'f:2', 'k:>', 'f:3', 'p:shot'],
    'gb_mode':        ['k:A', 'f:30', 'k:-', 'f:3', 'p:shot'],
    'screen_off':     ['k:s', 'f:30', 'p:shot'],
    'screen_black':   ['k:sv', 'f:30', 'p:shot'],
    'pairing_menu':   ['k:S', 'f:30', 'k:-', 'f:3', 'k:>', 'f:2', 'k:-', 'f:2', 'k:v', 'f:2', 'k:-', 'f:3', 'p:shot'],
    'pairing_sent':   ['k:S', 'f:30', 'k:-', 'f:3', 'k:A', 'f:2', 'k:-', 'f:5', 'p:shot'],
}


def render(harness, rom, steps):
    with tempfile.TemporaryDirectory() as d:
        r = subprocess.run([harness, rom, d] + steps, capture_output=True, text=True)
        if r.returncode: raise RuntimeError(f'harness: {r.stderr.strip()}')
        return read_ppm(os.path.join(d, 'shot.ppm'))


def diff_image(w, h, gold, act):
    out, changed = bytearray(), 0
    for i in range(w * h):
        g, a = gold[i * 3:i * 3 + 3], act[i * 3:i * 3 + 3]
        if g != a:
            changed += 1
            out += b'\xff\x00\x00'
        else:
            out += bytes(v // 3 for v in a)
    return changed, bytes(out)


def main():
    harness, rom, ver = sys.argv[1:4]
    approve = '--approve' in sys.argv
    gold_dir = os.path.join(HERE, 'visual', 'golden', ver)
    out_dir = os.path.join(os.path.dirname(HERE), 'build', 'visual', ver)
    os.makedirs(gold_dir, exist_ok=True)
    os.makedirs(out_dir, exist_ok=True)
    fails = 0
    for name, steps in SCREENS.items():
        w, h, act = render(harness, rom, steps)
        gold_path = os.path.join(gold_dir, name + '.png')
        write_png(os.path.join(out_dir, name + '.actual.png'), w, h, act)
        if approve:
            shutil.copy(os.path.join(out_dir, name + '.actual.png'), gold_path)
            print(f'APPROVED {ver}/{name}')
            continue
        if not os.path.exists(gold_path):
            print(f'FAIL  {ver}/{name:16} no golden (review build/visual/{ver}/{name}.actual.png, then make visual-approve)')
            fails += 1
            continue
        gw, gh, gold = read_png(gold_path)
        if (gw, gh) != (w, h):
            print(f'FAIL  {ver}/{name:16} size {w}x{h} != golden {gw}x{gh}')
            fails += 1
            continue
        changed, dimg = diff_image(w, h, gold, act)
        if changed:
            write_png(os.path.join(out_dir, name + '.diff.png'), w, h, dimg)
            print(f'FAIL  {ver}/{name:16} {changed} pixels differ -> build/visual/{ver}/{name}.diff.png')
            fails += 1
        else:
            print(f'PASS  {ver}/{name}')
    if not approve:
        print(f'\nvisual {ver}: {len(SCREENS) - fails}/{len(SCREENS)} screens match their goldens')
    sys.exit(1 if fails else 0)


if __name__ == '__main__':
    main()

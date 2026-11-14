#!/usr/bin/env python3
"""Regression test: does our ROM put the same protocol stream on the cart bus as the stock ROM?

Runs the ROM headless in mGBA (tools/emu_harness.c) through the same key scenarios that produced
tests/baselines/v1.1a/*.log (recorded from the stock v1.1a ROM). For each scenario it
compares:
  - the preamble exactly: sync byte, then GB-mode pair or pairing message if present
  - the key-word stream as a sequence of (hi, lo) pairs, collapsing consecutive repeats (timing
    differs slightly between ROMs: stock is about 2.1 ms, ours 2.0 ms, so counts differ but order mustn't)
  - the send rate: 7-9 key words per frame (about 2 ms)

usage: run_tests.py <emu_harness> <rom.gba> <baseline_dir>
"""
import os, subprocess, sys, tempfile

SYNC, GB, CFG = 0x49, (0x63, 0x5A), (0x6D, 0x52)   # v1.1a

# name: (our key script, note). Same scripts as the stock recordings, except 03 (see note).
TAP_RIGHT = ['k:>', 'f:3', 'k:-', 'f:6']
SCENARIOS = {
    '01_normal':     (['f:60', 'k:A', 'f:10', 'k:-', 'f:10'], ''),
    '02_showkeys':   (['k:B', 'f:60', 'k:-', 'f:5', 'k:A', 'f:5', 'k:^>', 'f:5', 'k:Ss', 'f:5',
                       'k:lr', 'f:5', 'k:lrA', 'f:5'], 'stock B-at-boot key letters dropped; bytes identical'),
    '03_config':     (['k:S', 'f:60', 'k:-', 'f:10'] + TAP_RIGHT * 6 +
                      ['k:v', 'f:3', 'k:-', 'f:6', 'k:v', 'f:3', 'k:-', 'f:6', 'k:A', 'f:5', 'k:-', 'f:30'],
                      'stock reached CH 7 via auto-repeat; we tap Right 6x to reach the same values'),
    '04_screen_off': (['k:s', 'f:60', 'k:-', 'f:5', 'k:A', 'f:5'], ''),
    '05_black':      (['k:sv', 'f:60', 'k:-', 'f:5'], ''),
    '06_gbmode':     (['k:A', 'f:60', 'k:-', 'f:5'], ''),
}


def read_log(path):
    rows = []
    for line in open(path):
        if line.startswith('#'): continue
        f = line.split()
        if f[2] == '??': raise SystemExit(f'{path}: unrecognised store: {line.strip()}')
        rows.append((int(f[0]), int(f[2], 16)))
    return rows


def parse(rows):
    """-> (preamble tuple, collapsed pair list, pairs per frame)"""
    vals = [v for _, v in rows]
    if not vals or vals[0] != SYNC: return ('NO SYNC; first writes:',) + tuple(vals[:4]), [], 0
    pre, i = [SYNC], 1
    if tuple(vals[1:3]) == GB: pre += vals[1:3]; i = 3
    elif tuple(vals[1:3]) == CFG: pre += vals[1:6]; i = 6
    pairs = [tuple(vals[j:j + 2]) for j in range(i, len(vals) - 1, 2)]
    collapsed = [p for k, p in enumerate(pairs) if k == 0 or p != pairs[k - 1]]
    frames = {}
    for f, _ in rows[i:]: frames[f] = frames.get(f, 0) + 1
    mid = sorted(frames)[2:-2] if len(frames) > 6 else []
    rate = (sum(frames[f] for f in mid) / len(mid) / 2) if mid else 0
    return tuple(pre), collapsed, rate


def fmt(seq):
    return [hex(v) if isinstance(v, int) else v for v in seq]


def run(harness, rom, script):
    with tempfile.TemporaryDirectory() as d:
        subprocess.run([harness, rom, d] + script, check=True, capture_output=True)
        return read_log(os.path.join(d, 'cart_writes.log'))


def main():
    harness, rom, base = sys.argv[1:4]
    fails = 0
    for name, (script, note) in SCENARIOS.items():
        s_pre, s_pairs, s_rate = parse(read_log(os.path.join(base, name + '.log')))
        o_pre, o_pairs, o_rate = parse(run(harness, rom, script))
        problems = []
        if o_pre != s_pre: problems.append(f'preamble {fmt(o_pre)} != stock {fmt(s_pre)}')
        if o_pairs != s_pairs: problems.append(f'key pairs {o_pairs} != stock {s_pairs}')
        if s_pairs and not (7 <= o_rate <= 9): problems.append(f'rate {o_rate:.1f} key words/frame (want 7-9)')
        ok = not problems
        fails += not ok
        print(f"{'PASS' if ok else 'FAIL'}  {name:14} pre={fmt(o_pre)} pairs={len(o_pairs)} "
              f"rate={o_rate:.1f}/frame (stock {s_rate:.1f})" + (f'  [{note}]' if note else ''))
        for p in problems: print('      ' + p)
    print(f'\n{len(SCENARIOS) - fails}/{len(SCENARIOS)} scenarios match the stock ROM')
    sys.exit(1 if fails else 0)


if __name__ == '__main__':
    main()

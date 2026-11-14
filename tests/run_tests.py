#!/usr/bin/env python3
"""Regression + timing test: does our ROM drive the cart bus like the stock ROM, within budget?

    python3 tests/run_tests.py <emu_harness> <rom.gba> <v1.1a|v1.1b>

Runs every scenario in tests/scenarios.py headless in mGBA and checks, per scenario:
  bus      every cart store is a 16-bit STRH to exactly 0x08000000 (no 8/32-bit, no other address)
  preamble sync byte, then GB-mode pair or pairing message, identical to the stock recording
  framing  after the preamble writes come in hi/lo pairs 0.12-0.5 ms apart (stray byte, missing delay)
  pairs    the key-word stream, as (hi, lo) pairs with consecutive repeats collapsed, matches stock
  gap      no gap between consecutive key words > 2.2 ms (SPEC.md §0 rule 7)
  latency  every key change appears on the bus within 2.2 ms (press-to-send)
  rate     every frame between the first and last key word carries 7-9 key words (stalls show up as 0)
"""
import os, subprocess, sys, tempfile
from scenarios import SCENARIOS, PROTOCOLS, CPU_HZ, MAX_GAP_MS, MAX_PAIR_MS, MIN_PAIR_MS, pack, read_log

HERE = os.path.dirname(os.path.abspath(__file__))


def fmt(seq):
    return [hex(v) if isinstance(v, int) else v for v in seq]


PAIR = MAX_PAIR_MS / 1000 * CPU_HZ
MINPAIR = MIN_PAIR_MS / 1000 * CPU_HZ


def analyse(writes, proto, end=None):
    """-> dict(pre, pairs=[(frame, cycle, word, (hi, lo))], errs)"""
    errs = [f'bad store: width {w} at {a:#010x} value {v}' for _, _, a, w, v in writes
            if w != 16 or a != 0x08000000 or v is None]
    vals = [v for *_, v in writes]
    if not vals or vals[0] != proto['sync']:
        return dict(pre=('NO SYNC; first writes:',) + tuple(vals[:4]), pairs=[], errs=errs)
    i = 1
    if tuple(vals[1:3]) == proto['gb']: i = 3
    elif tuple(vals[1:3]) == proto['cfg']: i = 6
    for k in range(i - 1):  # writes inside the preamble use the same write+delay routine (chunk1 F4)
        dt = writes[k + 1][1] - writes[k][1]
        if not MINPAIR <= dt <= PAIR:
            errs.append(f'preamble spacing {dt / CPU_HZ * 1000:.3f} ms between writes {k} and {k + 1}')
    rest = writes[i:]
    if len(rest) % 2:
        # odd count: fine only if the last write is a hi half cut off by the end of the run
        if end is None or end - rest[-1][1] > PAIR:
            errs.append(f'framing: {len(rest)} writes after the preamble, stray half pair at cycle {rest[-1][1]}')
        rest = rest[:-1]
    for j in range(0, len(rest) - 1, 2):     # each hi must be followed closely by its lo
        dt = rest[j + 1][1] - rest[j][1]
        if dt > PAIR:
            errs.append(f'framing: hi->lo spacing {dt / CPU_HZ * 1000:.3f} ms at frame {rest[j][0]} (stray byte?)')
            break
        if dt < MINPAIR:
            errs.append(f'write delay: hi->lo spacing {dt / CPU_HZ * 1000:.3f} ms < {MIN_PAIR_MS} ms (delay loop shortened?)')
            break
    pairs = [(rest[j][0], rest[j][1], (rest[j][4] << proto['shift']) | rest[j + 1][4], (rest[j][4], rest[j + 1][4]))
             for j in range(0, len(rest) - 1, 2)]
    return dict(pre=tuple(vals[:i]), pairs=pairs, errs=errs)


def collapsed(pairs):
    hl = [p[3] for p in pairs]
    return [p for k, p in enumerate(hl) if k == 0 or p != hl[k - 1]]


def timing(pairs, keys, proto):
    """-> (max gap ms, max press-to-send ms, rate problems)"""
    if len(pairs) < 2: return 0.0, 0.0, []
    cyc = [p[1] for p in pairs]
    gap = max(b - a for a, b in zip(cyc, cyc[1:]))
    lat = 0
    for _, kc, km in keys:
        if kc < cyc[0] or kc > cyc[-1]: continue
        want = pack(km, proto['xy'])
        hit = next((p for p in pairs if p[1] >= kc and p[2] == want), None)
        lat = max(lat, (hit[1] - kc) if hit else 10 ** 12)
    per = {}
    for f, *_ in pairs: per[f] = per.get(f, 0) + 1
    f0, f1 = pairs[0][0], pairs[-1][0]
    rate = [f'frame {f}: {per.get(f, 0)} key words' for f in range(f0 + 1, f1) if not 7 <= per.get(f, 0) <= 9]
    ms = lambda c: c / CPU_HZ * 1000
    return ms(gap), ms(lat), rate


def main():
    harness, rom, ver = sys.argv[1:4]
    proto = PROTOCOLS[ver]
    base = os.path.join(HERE, 'baselines', ver)
    fails = 0
    for name, sc in SCENARIOS.items():
        s_w, s_k, s_e = read_log(os.path.join(base, name + '.log'))
        with tempfile.TemporaryDirectory() as d:
            r = subprocess.run([harness, rom, d] + sc['ours'], capture_output=True, text=True)
            if r.returncode:
                print(f'FAIL  {name:14} harness error: {r.stderr.strip() or r.returncode}')
                fails += 1
                continue
            o_w, o_k, o_e = read_log(os.path.join(d, 'cart_writes.log'))
        s, o = analyse(s_w, proto, s_e), analyse(o_w, proto, o_e)
        probs = list(o['errs'])
        if o['pre'] != s['pre']: probs.append(f"preamble {fmt(o['pre'])} != stock {fmt(s['pre'])}")
        if sc.get('sync_after_last_key') and o_w and o_k and o_w[0][1] < o_k[-2][1]:
            probs.append('boot order: sync was sent before the final key press (SPEC §S: Settings sends nothing)')
        if collapsed(o['pairs']) != collapsed(s['pairs']):
            probs.append(f"pairs {collapsed(o['pairs'])} != stock {collapsed(s['pairs'])}")
        gap, lat, rate = timing(o['pairs'], o_k, proto)
        sgap, slat, _ = timing(s['pairs'], s_k, proto)
        if gap > MAX_GAP_MS: probs.append(f'gap {gap:.2f} ms > {MAX_GAP_MS} ms')
        if lat > MAX_GAP_MS: probs.append(f'press-to-send {lat:.2f} ms > {MAX_GAP_MS} ms')
        probs += [f'rate: {r}' for r in rate[:3]]
        fails += bool(probs)
        t = f'gap {gap:.2f}ms lat {lat:.2f}ms (stock {sgap:.2f}/{slat:.2f})' if o['pairs'] else 'no key words'
        print(f"{'PASS' if not probs else 'FAIL'}  {name:14} pre={fmt(o['pre'])} pairs={len(collapsed(o['pairs']))} {t}"
              + (f"  [{sc['note']}]" if sc.get('note') else ''))
        for p in probs: print('      ' + p)
    print(f'\n{ver}: {len(SCENARIOS) - fails}/{len(SCENARIOS)} scenarios match the stock ROM within the timing budget')
    sys.exit(1 if fails else 0)


if __name__ == '__main__':
    main()

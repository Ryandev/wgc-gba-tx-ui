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
SEND_PERIOD_CYCLES = 524 * 64  # timer 3 at 16.78 MHz / 64, reload 0x10000 - 524 (src/link.c)


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
        ours_only = 'expect' in sc  # a feature stock doesn't have: checked against `expect`, not stock
        s_w, s_k, s_e = ([], [], None) if ours_only else read_log(os.path.join(base, name + '.log'))
        with tempfile.TemporaryDirectory() as d:
            r = subprocess.run([harness, rom, d] + sc['ours'], capture_output=True, text=True)
            if r.returncode:  # the harness itself failed: an infrastructure error, not a test result
                print(f'ERROR {name:14} harness error: {r.stderr.strip() or r.returncode}')
                sys.exit(3)
            o_w, o_k, o_e = read_log(os.path.join(d, 'cart_writes.log'))
        s, o = analyse(s_w, proto, s_e), analyse(o_w, proto, o_e)
        probs = list(o['errs'])
        if ours_only:
            if o['pre'] != sc['pre'][ver]:
                probs.append(f"preamble {fmt(o['pre'])} != {fmt(sc['pre'][ver])}")
        elif o['pre'] != s['pre']:
            probs.append(f"preamble {fmt(o['pre'])} != stock {fmt(s['pre'])}")
        if sc.get('nothing_before_frame') and o_w and o_w[0][0] < sc['nothing_before_frame']:
            probs.append(f"boot order: first write at frame {o_w[0][0]} < {sc['nothing_before_frame']} (before PLAY)")
        if sc.get('sync_after_a') and o_w:
            a_press = next((c for _, c, m in o_k if m & 1), None)  # first A press (KEYINPUT bit 0)
            if a_press is None or o_w[0][1] < a_press:
                probs.append('boot order: sync sent before A was pressed (SPEC §S: Settings sends nothing)')
        for k in range(len(o_w) - 1):  # every consecutive pair of writes, incl. preamble -> first word (F8)
            if o_w[k + 1][1] - o_w[k][1] < MINPAIR:
                probs.append(f'write spacing {(o_w[k + 1][1] - o_w[k][1]) / CPU_HZ * 1000:.3f} ms < {MIN_PAIR_MS} ms at write {k}')
                break
        if ours_only:
            words = [w for k, w in enumerate(p[2] for p in o['pairs']) if k == 0 or w != o['pairs'][k - 1][2]]
            want = [pack(m, proto['xy']) for m in sc['expect']]
            if words != want:
                probs.append(f'key words {fmt(words)} != expected {fmt(want)}')
        elif collapsed(o['pairs']) != collapsed(s['pairs']):
            probs.append(f"pairs {collapsed(o['pairs'])} != stock {collapsed(s['pairs'])}")
        # press-to-send: only keys pressed while input passes through (`lat_keys` limits it for ours-only
        # scenarios, where presses inside Settings are deliberately not sent)
        lat_keys = [k for k in o_k if k[0] >= sc.get('lat_from_frame', 0)]
        gap, lat, rate = timing(o['pairs'], lat_keys, proto)
        sgap, slat, _ = timing(s['pairs'], s_k, proto)
        if gap > MAX_GAP_MS: probs.append(f'gap {gap:.2f} ms > {MAX_GAP_MS} ms')
        if len(o['pairs']) > 2:  # the send period is a design value: 524 ticks of 64 cycles (re-check F4)
            gaps = sorted(b[1] - a[1] for a, b in zip(o['pairs'], o['pairs'][1:]))
            median = gaps[len(gaps) // 2]
            if abs(median - SEND_PERIOD_CYCLES) > SEND_PERIOD_CYCLES * 0.0005:
                probs.append(f'send period {median} cycles != {SEND_PERIOD_CYCLES} (+-0.05%)')
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
    # Exit codes: 0 = pass, 1 = a scenario failed, 3 = infrastructure error (never a test result)
    try:
        main()
    except SystemExit:
        raise
    except Exception:  # noqa: BLE001 - anything unexpected is infrastructure, not a kill
        import traceback
        traceback.print_exc()
        sys.exit(3)

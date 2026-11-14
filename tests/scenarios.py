"""Shared test definitions: protocol parameters, key scenarios, key-word packing, log parsing.

Used by tests/record_baselines.py (stock ROM -> tests/baselines/<ver>/) and tests/run_tests.py
(our ROM vs those baselines).
"""

CPU_HZ = 16_777_216
MAX_GAP_MS = 2.2          # SPEC.md §0 rule 7: gap between key words and press-to-send budget
MAX_PAIR_MS = 0.5         # hi -> lo spacing inside one key word: upper bound (stray-byte detection)
MIN_PAIR_MS = 0.12        # ... lower bound: stock 0.136 ms, ours 0.134 ms. Catches a shortened or
                          # removed delay loop, which would overrun the cart's 8 MHz ATtiny.

PROTOCOLS = {
    'v1.1a': dict(sync=0x49, gb=(0x63, 0x5A), cfg=(0x6D, 0x52), shift=7, xy=False),
    'v1.1b': dict(sync=0x29, gb=(0x23, 0x1A), cfg=(0x2D, 0x12), shift=6, xy=True),
}

TAP = lambda k: [f'k:{k}', 'f:3', 'k:-', 'f:6']

# name -> dict(ours=[steps], stock=[steps] (if different), note=str)
SCENARIOS = {
    '01_normal':     dict(ours=['f:60', 'k:A', 'f:10', 'k:-', 'f:10']),
    '02_showkeys':   dict(ours=['k:B', 'f:60', 'k:-', 'f:5', 'k:A', 'f:5', 'k:^>', 'f:5', 'k:Ss', 'f:5',
                                'k:lr', 'f:5', 'k:lrA', 'f:5'],
                          note='stock B-at-boot key letters dropped; bytes identical'),
    '03_config':     dict(ours=['k:S', 'f:60', 'k:-', 'f:10'] + TAP('>') * 6 + TAP('v') * 2 + ['k:A', 'f:5', 'k:-', 'f:30'],
                          stock=['k:S', 'f:60', 'k:-', 'f:10', 'k:>', 'f:20', 'k:-', 'f:10', 'k:>', 'f:20', 'k:-',
                                 'f:10', 'k:v', 'f:20', 'k:-', 'f:10', 'k:v', 'f:20', 'k:-', 'f:10', 'k:A', 'f:20',
                                 'k:-', 'f:30'],
                          sync_after_last_key=True,  # SPEC §S: boot-time Settings sends nothing until A (pairing)
                          note='stock reaches CH 7 via auto-repeat; ours taps Right 6x for the same values'),
    '04_screen_off': dict(ours=['k:s', 'f:60', 'k:-', 'f:5', 'k:A', 'f:5']),
    '05_black':      dict(ours=['k:sv', 'f:60', 'k:-', 'f:5']),
    '06_gbmode':     dict(ours=['k:A', 'f:60', 'k:-', 'f:5']),
    '08_each_button': dict(ours=['f:30'] + [st for k in ['A', 'B', 's', 'S', '>', '<', '^', 'v', 'r', 'l']
                                            for st in (f'k:{k}', 'f:4', 'k:-', 'f:4')],
                           note='every button alone (mutation testing found Left was never exercised)'),
    '07_xy':         dict(ours=['f:30', 'k:lrA', 'f:5', 'k:lrB', 'f:5', 'k:lrAB', 'f:5', 'k:-', 'f:5'],
                          note='L+R+A/B -> X/Y on v1.1b; plain L+R+A/B on v1.1a'),
}

KEYINPUT_TO_W = [(0, 0x001), (1, 0x002), (4, 0x004), (5, 0x008), (6, 0x010), (7, 0x020),
                 (2, 0x040), (3, 0x080), (9, 0x100), (8, 0x200)]   # (KEYINPUT bit, key-word bit)


def pack(keys, xy):
    """KEYINPUT-order pressed mask -> 12-bit key word (mirrors src/link.c link_pack)."""
    w = 0
    for kb, wb in KEYINPUT_TO_W:
        if keys >> kb & 1: w |= wb
    if xy and (keys >> 8 & 1) and (keys >> 9 & 1):
        if keys & 1: w |= 0x400
        if keys & 2: w |= 0x800
    return w


def read_log(path):
    """-> (writes [(frame, cycle, addr, width, value|None)], keys [(frame, cycle, keymask)], end cycle)"""
    writes, keys, end = [], [], None
    for line in open(path):
        f = line.split()
        if not f or f[0].startswith('#'): continue
        if f[0] == 'W':
            writes.append((int(f[1]), int(f[2]), int(f[3], 16), int(f[4]), None if f[5] == '??' else int(f[5], 16)))
        elif f[0] == 'K':
            keys.append((int(f[1]), int(f[2]), int(f[3], 16)))
        elif f[0] == 'E':
            end = int(f[2])
    return writes, keys, end

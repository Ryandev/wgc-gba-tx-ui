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
    # Pairing with three distinct values (7, 4, 2) so swapped arguments can't pass (chunk2 F4), and a
    # B press afterwards: nothing may be sent after the pairing message (chunk2 F8).
    '03_config':     dict(ours=['k:S', 'f:60', 'k:-', 'f:10'] + TAP('v') + TAP('A') + TAP('>') * 6 + TAP('v') + TAP('>') * 3 + TAP('v')
                           + TAP('>') + ['k:A', 'f:20', 'k:-', 'f:10', 'k:B', 'f:10', 'k:-', 'f:20'],
                          stock=['k:S', 'f:60', 'k:-', 'f:10', 'k:>', 'f:20', 'k:-', 'f:10', 'k:>', 'f:20', 'k:-',
                                 'f:10', 'k:v', 'f:3', 'k:-', 'f:20', 'k:>', 'f:20', 'k:-', 'f:20', 'k:v', 'f:3',
                                 'k:-', 'f:20', 'k:>', 'f:3', 'k:-', 'f:20', 'k:A', 'f:20', 'k:-', 'f:10', 'k:B',
                                 'f:10', 'k:-', 'f:20'],
                          sync_after_a=True,  # SPEC §S: boot-time Settings sends nothing until A (pairing)
                          note='pairs CH 7 / ADDR 4 / PKTS 2 (stock via auto-repeat, ours via Settings > PAIRING), then B: no sends'),
    '04_screen_off': dict(ours=['k:s', 'f:60', 'k:-', 'f:5', 'k:A', 'f:5']),
    '05_black':      dict(ours=['k:sv', 'f:60', 'k:-', 'f:5']),
    '06_gbmode':     dict(ours=['k:A', 'f:60', 'k:-', 'f:5']),
    '08_each_button': dict(ours=['f:30'] + [st for k in ['A', 'B', 's', 'S', '>', '<', '^', 'v', 'r', 'l']
                                            for st in (f'k:{k}', 'f:4', 'k:-', 'f:4')],
                           note='every button alone (mutation testing found Left was never exercised)'),
    # GB mode with L/R pressed: the LCD note renders, counts down and hides while keys keep moving, so
    # SPEC §0 rule 7 (gap, press-to-send) is checked with the heaviest UI work running (phase1-chunk2 F1)
    '09_gbmode_lr':  dict(ours=['k:A', 'f:60', 'k:-', 'f:5', 'k:l', 'f:2', 'k:lB', 'f:3', 'k:-', 'f:3', 'k:r', 'f:5',
                                'k:rA', 'f:3', 'k:-', 'f:200', 'k:^', 'f:3', 'k:-', 'f:5'],
                          note='GB mode, L/R tapped: note render + countdown + hide during key traffic'),
    # The whole splash unskipped (title and tip pages render, antenna included), then keys: the timing
    # checks cover every splash phase (review phase1-chunk3 F3)
    '10_splash_full': dict(ours=['f:200', 'k:A', 'f:5', 'k:-', 'f:5', 'k:^B', 'f:5', 'k:-', 'f:5'],
                           note='splash runs to the end (fade, power, title, tip), then keys'),
    # In-session Settings (SPEC §N): hold L+R+Select 1 s; from then on only neutral words while the menu
    # is used (Down, A into PAIRING, B, Down, A into ABOUT, B, B to play); after the zoom back out,
    # keys pass through again. Ours only: stock has no such overlay, so it's checked against `expect`.
    '11_settings':   dict(ours=['f:200', 'k:lrs', 'f:70', 'k:-', 'f:25'] + TAP('v') + TAP('A') + TAP('B') + TAP('v')
                          + TAP('A') + TAP('B') + TAP('B') + ['f:30', 'k:^', 'f:5', 'k:-', 'f:5'],
                          expect=[0, 0x304, 0, 0x40, 0], pre={'v1.1a': (0x49,), 'v1.1b': (0x29,)},
                          lat_from_frame=380, note='Settings open: only neutral words; pass-through after'),
    # Boot-time Settings -> PLAY (SPEC §S, review phase1-chunk4 F2/F3): Start + A held at boot (Settings,
    # GB mode); nothing is written before PLAY (Start at frame ~50); then sync + GB pair, neutral words
    # through the fade/zoom out (the held Start isn't sent), then pass-through.
    '12_boot_play':  dict(ours=['k:SA', 'f:30', 'k:-', 'f:20', 'k:S', 'f:3', 'k:-', 'f:40', 'k:^', 'f:5', 'k:-', 'f:5'],
                          expect=[0, 0x40, 0], pre={'v1.1a': (0x49, 0x63, 0x5A), 'v1.1b': (0x29, 0x23, 0x1A)},
                          nothing_before_frame=50, lat_from_frame=90,
                          note='boot-time Settings: nothing before PLAY, neutral while zooming out'),
    # Boot-time Settings: INPUT > GB MODE on, then Start (PLAY): sync then the GB pair (SPEC §G)
    '13_boot_gb':    dict(ours=['k:S', 'f:30', 'k:-', 'f:3'] + TAP('v') * 3 + TAP('A') + TAP('>') + TAP('B') + ['k:S', 'f:3', 'k:-',
                          'f:40', 'k:^', 'f:5', 'k:-', 'f:5'],
                          expect=[0, 0x40, 0], pre={'v1.1a': (0x49, 0x63, 0x5A), 'v1.1b': (0x29, 0x23, 0x1A)},
                          lat_from_frame=120, note='GB mode set in boot-time Settings: GB pair at PLAY'),
    # Asleep (SPEC §K, black after 2 min idle): the cart keeps sending, and the waking press is sent as
    # usual within the timing budget
    '14_idle_sleep': dict(ours=['f:7600', 'k:A', 'f:5', 'k:-', 'f:5', 'k:^', 'f:5', 'k:-', 'f:5'],
                          expect=[0, 0x01, 0, 0x40, 0], pre={'v1.1a': (0x49,), 'v1.1b': (0x29,)},
                          note='asleep (black screen): keys still sent, the wake press too'),
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

#!/usr/bin/env python3
"""Golden-image (screenshot) tests: render screens in headless mGBA and compare pixel-exact.

    python3 tests/visual.py <emu_harness> <rom.gba> <v1.1a|v1.1b> [--approve] [--screen=a,b] [--out=DIR]

mGBA is deterministic: the same ROM + key script gives identical pixels every run, so the check is
exact (no fuzzy threshold). On mismatch it writes build/visual/<ver>/<name>.actual.png and
<name>.diff.png (changed pixels in red over a dimmed copy) and fails. --approve copies the
renders over the goldens, only for captures that changed. Only do that after looking at them.
Goldens: tests/visual/golden/<ver>/<name>.png (our own renders, fine to publish).
"""
import os, shutil, subprocess, sys, tempfile
from pngio import read_png, write_png, read_ppm

HERE = os.path.dirname(os.path.abspath(__file__))

# name -> harness steps; every p:<capture> becomes golden <name>__<capture>.png (several per screen).
SCREENS = {
    # boot frames: 1-7 are white in stock too (crt0, forced blank); frame 8 must already be the whole,
    # correct picture, never garbage backdrop or a torn image (review phase1-chunk1 F1)
    'boot':    ['f:7', 'p:frame7', 'f:1', 'p:frame8', 'f:22', 'p:splash_fade', 'f:20', 'p:splash_power',
                'f:40', 'p:splash_title', 'f:60', 'p:splash_tip', 'f:60', 'p:idle'],
    # boot splash (SPEC §B): frame 8 = its first, blank frame; fade, power-on, title, tip, then the main
    # screen. Any NEW key press skips it at once (and the press still lights, and is sent).
    'splash_skip': ['f:20', 'k:B', 'f:2', 'p:b_skips'],
    # keys held from boot (A for GB mode, plus L) neither light nor strike during the splash; after it,
    # the still-held A lights (review phase1-chunk3 F1)
    'splash_held': ['k:Al', 'f:30', 'k:A', 'f:25', 'p:power', 'f:80', 'p:tip', 'f:60', 'p:after'],
    # Captures wait 7 frames after a change (> VIEW_FADE_FRAMES = 6), so earlier presses have fully
    # faded and each shot shows only what's held (review phase1-chunk1 F5).
    # each D-pad direction captured on its own (chunk3 F1: a single final shot only checked Right)
    'dpad':    ['f:30', 'k:^', 'f:7', 'p:up', 'k:v', 'f:7', 'p:down', 'k:<', 'f:7', 'p:left', 'k:>', 'f:7', 'p:right'],
    # chords: every button in several combinations (chosen in Phase 0 so the hex readout showed 0-F)
    'chords':  ['f:30', 'k:lrA', 'f:7', 'p:lra', 'k:lrAB', 'f:7', 'p:lrab', 'k:sSB', 'f:7', 'p:ssb',
                'k:Av<s', 'f:7', 'p:a_down_left_sel', 'k:r^<S', 'f:7', 'p:r_up_left_start',
                'k:B>v', 'f:7', 'p:b_right_down', 'k:A>', 'f:7', 'p:hex5', 'k:B<', 'f:7', 'p:hexA',
                'k:AB<', 'f:7', 'p:hexB', 'k:A><', 'f:7', 'p:hexD', 'k:B><', 'f:7', 'p:hexE', 'k:^v<>', 'f:7', 'p:hex3C',
                'k:-', 'f:7', 'p:released'],
    # GB mode: quiet (amber LED only) until L/R; then L/R light amber (pressed but not sent), the other
    # is struck through and the LCD note counts down 3 s from the last L/R press, then all hides
    # (A held at boot doesn't skip the splash: splash_kept is its power-on, LED amber; shown waits for
    # the main screen; review phase1-chunk3 F4)
    'gb_mode': ['k:A', 'f:30', 'k:-', 'f:15', 'p:splash_kept', 'f:172', 'p:shown', 'k:l', 'f:7', 'p:l_amber', 'k:r', 'f:7', 'p:r_amber',
                'k:-', 'f:3', 'p:r_fading', 'f:4', 'p:faded', 'f:83', 'p:note_half', 'f:88', 'p:note_last',
                'f:4', 'p:note_gone'],
    # frames counted from the last L/R release (phase1-chunk2 F4): note_half at 90, note_last at 178 (still
    # shown: the timer runs from the release), note_gone at 182.
    # A button pressed while a note renders still lights at once (phase1-chunk2 F1: the old per-pixel
    # render took over a frame and B lit one frame late). Same timing as dpad's press-to-capture.
    'note_render': ['k:A', 'f:30', 'k:-', 'f:7', 'k:l', 'f:1', 'k:lB', 'f:2', 'p:b_lit'],
    # release fade: 6 steps (VIEW_FADE_FRAMES), seen one frame after the release (the release reaches
    # the sent word within 2 ms; the palette is written at the next VBlank). So: half way at 3 frames,
    # the last faint step at 6, exactly back to boot__idle at 7. A longer fade fails a_gone.
    'fade':    ['f:30', 'k:A', 'f:7', 'k:-', 'f:3', 'p:a_half', 'f:3', 'p:a_last', 'f:1', 'p:a_gone'],
    'off':     ['k:s', 'f:30', 'p:grey'],
    # frame 9 = first frame after startup; frames 1-7 are white in stock too (crt0 clears RAM under forced
    # blank before main), so they're not a useful golden (see SPEC.md, boot flash note)
    'black':   ['k:sv', 'f:9', 'p:frame9', 'f:21', 'p:frame30'],
    # Boot-time Settings (Start held, SPEC §S): starts zoomed into the LCD; frame 7 under forced blank
    # (colours only: the boot boundary moves with code size); home, PAIRING edited, SENT
    'settings_boot': ['k:S', 'f:7', 'p:frame7_colours', 'f:23', 'k:-', 'f:3', 'p:home', 'k:v', 'f:2', 'k:-', 'f:2',
                      'k:A', 'f:2', 'k:-', 'f:3', 'p:pairing', 'k:>', 'f:2', 'k:-', 'f:2', 'k:v', 'f:2', 'k:-', 'f:2',
                      'k:<', 'f:2', 'k:-', 'f:3', 'p:edited', 'k:A', 'f:2', 'k:-', 'f:5', 'p:sent'],
    # 3-digit values (review phase1-chunk4 F1: the hundreds digit was dropped): hold Right on CHANNEL and
    # on ADDRESS (auto-repeat) past 100 (v1.1a; v1.1b stops at 63)
    'settings_3digit': ['k:S', 'f:30', 'k:-', 'f:3', 'k:v', 'f:2', 'k:-', 'f:2', 'k:A', 'f:2', 'k:-', 'f:3',
                        'k:>', 'f:900', 'k:-', 'f:2', 'k:v', 'f:2', 'k:-', 'f:2', 'k:>', 'f:900', 'k:-', 'f:3', 'p:pairing'],
    # Boot-time Settings -> PLAY: zoom out to the controller with the LED green (sending), and in-session
    # Settings then opens as usual (review phase1-chunk4 F2/F3, mutation re-run)
    'settings_boot_play': ['k:S', 'f:30', 'k:-', 'f:3', 'k:S', 'f:3', 'k:-', 'f:30', 'p:main', 'k:lrs', 'f:61',
                           'k:-', 'f:25', 'p:in_session'],
    # GB mode: Settings opened and closed, then L tapped: the GB note shows whole (the LCD layer's map
    # is intact after the Settings layer was set up) and no note carried over from before Settings
    'settings_then_note': ['k:A', 'f:30', 'k:-', 'f:200', 'k:lrs', 'f:61', 'k:-', 'f:25', 'p:opened', 'k:B', 'f:3',
                           'k:-', 'f:30', 'p:closed', 'k:l', 'f:3', 'k:-', 'f:5', 'p:note'],
    # In-session Settings (SPEC §N): hold L+R+Select 1 s; zoom in (LED red from the first frame), the
    # pages, then out again and back to the controller with the LED green
    'settings': ['f:200', 'k:lrs', 'f:61', 'p:zoom_start', 'f:5', 'p:zoom_mid', 'k:-', 'f:20', 'p:home',
                 'k:v', 'f:3', 'k:-', 'f:3', 'k:A', 'f:3', 'k:-', 'f:3', 'p:pairing', 'k:B', 'f:3', 'k:-', 'f:3',
                 'k:v', 'f:3', 'k:-', 'f:3', 'k:A', 'f:3', 'k:-', 'f:3', 'p:about', 'k:B', 'f:3', 'k:-', 'f:3',
                 'k:B', 'f:3', 'k:-', 'f:4', 'p:fade_out', 'f:10', 'p:zoom_out', 'f:20', 'p:back'],
}


def captures(steps):
    return [st[2:] for st in steps if st.startswith('p:')]


def render(harness, rom, steps):
    """-> {capture: (w, h, rgb)}"""
    with tempfile.TemporaryDirectory() as d:
        # 'dirty': VRAM/palette start as garbage, like real hardware, so a missing clear is visible
        r = subprocess.run([harness, rom, d, 'dirty'] + steps, capture_output=True, text=True)
        if r.returncode: raise RuntimeError(f'harness: {r.stderr.strip()}')
        return {c: read_ppm(os.path.join(d, c + '.ppm')) for c in captures(steps)}


def colours(rgb):
    """The set of distinct colours in a raw RGB buffer."""
    return {bytes(rgb[i:i + 3]) for i in range(0, len(rgb), 3)}


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
    opts = dict(a.split('=', 1) for a in sys.argv[4:] if '=' in a)
    approve = '--approve' in sys.argv
    only = set(filter(None, opts.get('--screen', os.environ.get('SCREEN', '')).split(',')))
    gold_dir = os.path.join(HERE, 'visual', 'golden', ver)
    out_dir = opts.get('--out', os.path.join(os.path.dirname(HERE), 'build', 'visual', ver))
    os.makedirs(gold_dir, exist_ok=True)
    os.makedirs(out_dir, exist_ok=True)
    total = fails = 0
    for screen, steps in SCREENS.items():
        if only and screen not in only: continue
        for cap, (w, h, act) in render(harness, rom, steps).items():
            name, total = f'{screen}__{cap}', total + 1
            gold_path = os.path.join(gold_dir, name + '.png')
            act_path, diff_path = os.path.join(out_dir, name + '.actual.png'), os.path.join(out_dir, name + '.diff.png')
            write_png(act_path, w, h, act)
            if os.path.exists(diff_path): os.remove(diff_path)        # no stale diffs (chunk3 F6)
            gold = read_png(gold_path) if os.path.exists(gold_path) else None
            # captures named *_colours compare only WHICH colours appear, not where: for transition
            # frames whose exact scanline depends on boot timing (code size), where what matters is
            # that no garbage colour shows (phase1-chunk2)
            by_colour = cap.endswith('_colours')
            same = gold is not None and gold[:2] == (w, h) and (
                colours(gold[2]) == colours(act) if by_colour else gold[2] == act)
            if approve:
                if not same:
                    shutil.copy(act_path, gold_path)
                    print(f'APPROVED {ver}/{name}' + ('' if gold else ' (new)'))
                continue
            if gold is None:
                print(f'FAIL  {ver}/{name:28} no golden (look at {act_path}, then make visual-approve SCREEN={screen})')
                fails += 1
            elif gold[:2] != (w, h):
                print(f'FAIL  {ver}/{name:28} size {w}x{h} != golden {gold[0]}x{gold[1]}')
                fails += 1
            elif not same and by_colour:
                extra = sorted(c.hex() for c in colours(act) - colours(gold[2]))
                missing = sorted(c.hex() for c in colours(gold[2]) - colours(act))
                print(f'FAIL  {ver}/{name:28} colours differ: new {extra}, missing {missing}')
                fails += 1
            elif not same:
                changed, dimg = diff_image(w, h, gold[2], act)
                write_png(diff_path, w, h, dimg)
                print(f'FAIL  {ver}/{name:28} {changed} pixels differ -> {diff_path}')
                fails += 1
            else:
                print(f'PASS  {ver}/{name}')
    known = {f'{sc}__{c}.png' for sc, st in SCREENS.items() for c in captures(st)}
    orphans = sorted(set(os.listdir(gold_dir)) - known) if not only else []
    for o in orphans:
        print(f'FAIL  {ver}/{o} golden has no screen (delete it)')
    fails += len(orphans)
    if not approve:
        print(f'\nvisual {ver}: {total - fails}/{total} captures match their goldens')
    sys.exit(1 if fails else 0)


if __name__ == '__main__':
    # Exit codes: 0 = pass, 1 = a capture differs or is missing, 3 = infrastructure error
    try:
        main()
    except SystemExit:
        raise
    except Exception:  # noqa: BLE001 - harness crash, unreadable golden, ...: not a test result
        import traceback
        traceback.print_exc()
        sys.exit(3)

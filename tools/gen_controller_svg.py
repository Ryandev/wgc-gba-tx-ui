#!/usr/bin/env python3
"""Generate assets/controller.svg (the AGB-001 front) from the owner's GBA CAD.

    python3 tools/gen_controller_svg.py            # writes assets/controller.svg

Every shape comes from the owner's own 1:1 CAD model of the console (../CAD; the giant build is
that model at 9.8067x):
  - outlines of the shell, shoulders, side trims, lens, LCD window, LED and the button caps are
    front-view silhouettes traced by tools/cad_trace.py into assets/controller_cad.json;
  - pockets cut into the shell (speaker grille, START/SELECT recesses, top panel) aren't
    silhouettes, so they're drawn from the shell's own constants (CAD lib/part/shell.front.py),
    copied into SHELL_POCKETS below.
No third-party drawing is used, and no lettering or logos are drawn (owner decision, 2026-11-14).

Geometry is built in the CAD frame (mm, origin at the shell centre, y up) and mapped to SVG
(x right from the left-most point, y down from the top-most point) at the end.

Part ids are stable and semantic (svg_to_gba.py gives each its own palette entry):
  shoulder-l/r, trim-l/r, body, start-plate, select-plate, speaker-1..5, top-slot, lens, lcd,
  led, dpad-up/down/left/right/centre, btn-a, btn-b, btn-start, btn-select.
"""
import json
import math
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'assets', 'controller.svg')
TRACE = os.path.join(ROOT, 'assets', 'controller_cad.json')

# Pockets in the shell front, from CAD lib/part/shell.front.py (_GRILLE_*, _SS_*, _PANEL_*).
# CAD frame; angle in degrees, counter-clockwise from +x.
SHELL_POCKETS = {
    'grille': dict(cx=49.34, first_y=-21.5852, pitch=3.0, count=5, length=16.7243, width=1.5684, angle=13.9486),
    'ss':     dict(cx=-48.2488, first_y=-19.5206, pitch=8.0, count=2, length=16.9362, width=5.9819, angle=166.0597),
    'panel':  dict(cx=0.03905, cy=35.43665, length=18.5213, height=4.4913),
}
DPAD_ARM = 5.75   # width of each arm of the cross (CAD lib/part/dpad.py ARM_WIDTH)

CSS = ('.body{fill:#5040a8}.shoulder{fill:#382c80}.trim{fill:#40349a}.lens{fill:#202030}'
       '.lcd{fill:#b0d890}.button{fill:#3c3c4c}.plate{fill:#3c3090}.slot{fill:#281c60}.led{fill:#58e070}')


# ---- geometry helpers (CAD frame) ----------------------------------------------------------
def pill(cx, cy, length, width, angle_deg, steps=12):
    """A capsule (stadium) centred on (cx, cy), `length` overall, as a polygon."""
    a = math.radians(angle_deg)
    ux, uy = math.cos(a), math.sin(a)
    half, r = (length - width) / 2, width / 2
    pts = []
    for end, base in ((1, a - math.pi / 2), (-1, a + math.pi / 2)):
        ex, ey = cx + end * half * ux, cy + end * half * uy
        for i in range(steps + 1):
            t = base + math.pi * i / steps
            pts.append((ex + r * math.cos(t), ey + r * math.sin(t)))
    return pts


def clip(poly, inside, cross):
    """Sutherland-Hodgman: keep the part of poly where inside(p) holds; cross(p, q) = boundary point."""
    out = []
    for i, cur in enumerate(poly):
        prev = poly[i - 1]
        if inside(cur):
            if not inside(prev):
                out.append(cross(prev, cur))
            out.append(cur)
        elif inside(prev):
            out.append(cross(prev, cur))
    return out


def clip_x(poly, x, keep_above):
    def inside(p):
        return p[0] >= x if keep_above else p[0] <= x

    def cross(p, q):
        t = (x - p[0]) / (q[0] - p[0])
        return (x, p[1] + t * (q[1] - p[1]))
    return clip(poly, inside, cross)


def clip_y(poly, y, keep_above):
    def inside(p):
        return p[1] >= y if keep_above else p[1] <= y

    def cross(p, q):
        t = (y - p[1]) / (q[1] - p[1])
        return (p[0] + t * (q[0] - p[0]), y)
    return clip(poly, inside, cross)


def dpad_parts(outline):
    """Split the traced cross into four arms and the centre square, so each lights on its own."""
    xs, ys = [p[0] for p in outline], [p[1] for p in outline]
    cx, cy, a = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2, DPAD_ARM / 2
    centre = clip_y(clip_y(clip_x(clip_x(outline, cx - a, True), cx + a, False), cy - a, True), cy + a, False)
    return {
        'dpad-up': clip_y(outline, cy + a, True),
        'dpad-down': clip_y(outline, cy - a, False),
        'dpad-left': clip_x(outline, cx - a, False),
        'dpad-right': clip_x(outline, cx + a, True),
        'dpad-centre': centre,
    }


def f(x):
    return f'{x:.2f}'.rstrip('0').rstrip('.')


# ---- the drawing ---------------------------------------------------------------------------
def build(trace=None):
    if trace is None:
        with open(TRACE) as fh:
            trace = json.load(fh)
    t = {k: [tuple(p) for p in v['outline']] for k, v in trace['parts'].items()}
    parts = []   # (id, role, polygon in CAD frame), painter's order: first is at the back

    def add(pid, role, poly):
        parts.append((pid, role, poly))

    # behind the shell: only the rims that stick out past it show
    for side in ('l', 'r'):
        add(f'shoulder-{side}', 'shoulder', t[f'shoulder-{side}'])
    for side in ('l', 'r'):
        add(f'trim-{side}', 'trim', t[f'trim-{side}'])
    add('body', 'body', t['body'])
    ss, gr, pn = SHELL_POCKETS['ss'], SHELL_POCKETS['grille'], SHELL_POCKETS['panel']
    for i, name in enumerate(('select', 'start')):   # first_y is the lower recess (SELECT)
        add(f'{name}-plate', 'plate', pill(ss['cx'], ss['first_y'] + i * ss['pitch'], ss['length'], ss['width'], ss['angle']))
    for i in range(gr['count']):   # speaker-1 is the top slot
        y = gr['first_y'] + (gr['count'] - 1 - i) * gr['pitch']
        add(f'speaker-{i + 1}', 'slot', pill(gr['cx'], y, gr['length'], gr['width'], gr['angle']))
    add('top-slot', 'slot', pill(pn['cx'], pn['cy'], pn['length'], pn['height'], 0))
    add('lens', 'lens', t['lens'])
    add('lcd', 'lcd', t['lcd'])
    add('led', 'led', t['led'])
    for pid, poly in dpad_parts(t['dpad']).items():
        add(pid, 'button', poly)
    for pid in ('btn-a', 'btn-b', 'btn-start', 'btn-select'):
        add(pid, 'button', t[pid])

    # CAD frame -> SVG: x from the left-most point, y down from the top-most point
    allx = [x for _, _, poly in parts for x, _ in poly]
    ally = [y for _, _, poly in parts for _, y in poly]
    x0, y1 = min(allx), max(ally)
    w, h = max(allx) - x0, y1 - min(ally)

    def d(poly):
        return 'M' + ' L'.join(f'{f(x - x0)},{f(y1 - y)}' for x, y in poly) + ' Z'

    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {f(w)} {f(h)}" width="{f(w)}mm" height="{f(h)}mm">',
           '<!-- wgc-gba-tx-ui: Game Boy Advance (AGB-001) front, generated by tools/gen_controller_svg.py',
           '     from the owner\'s 1:1 CAD (outlines: assets/controller_cad.json; shell pockets:',
           '     lib/part/shell.front.py constants). Units: mm. Part ids are used by the asset pipeline;',
           '     colours here are only a preview. -->',
           f'<style>{CSS}</style>']
    out += [f'<path id="{pid}" class="{role}" d="{d(poly)}"/>' for pid, role, poly in parts]
    out.append('</svg>')
    return '\n'.join(out) + '\n', parts


if __name__ == '__main__':
    svg, parts = build()
    with open(OUT, 'w') as fh:
        fh.write(svg)
    print(f'wrote {os.path.relpath(OUT)} ({len(parts)} parts)')

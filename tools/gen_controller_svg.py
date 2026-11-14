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
    'panel':  dict(cx=0.03905, cy=35.43665, length=18.5213, height=4.4913),   # cy not used, see below
}
# Owner decision (2026-11-14): the top oval is centred vertically between the shell's top edge and
# the top of the lens, on the centre line (the CAD has it 1.2 mm lower).
DPAD_ARM = 5.75   # width of each arm of the cross (CAD lib/part/dpad.py ARM_WIDTH)

# Pixel shading, as in the approved mockups (mockups/make_mockups_cad.py); svg_to_gba.py applies it
# per pixel and gives each shade its own palette entry. By role:
SHADE = {
    'button':   {'data-shade': 'raised', 'data-hi': '#5c5c70', 'data-lo': '#24242c'},
    'shoulder': {'data-shade': 'raised', 'data-hi': '#6858c0', 'data-lo': '#281c60'},
    'body':     {'data-shade': 'rim', 'data-rim': '#281c60'},
    'trim':     {'data-shade': 'rim', 'data-rim': '#281c60'},
    'lens':     {'data-shade': 'top-hi', 'data-hi': '#303044'},
    'led':      {'data-glow-into': 'body'},
}

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


def clip(poly, a, b, c):
    """Sutherland-Hodgman: the part of poly where a*x + b*y + c >= 0 (repeated points removed)."""
    def side(p):
        return a * p[0] + b * p[1] + c

    out = []
    for i, cur in enumerate(poly):
        prev = poly[i - 1]
        sp, sc = side(prev), side(cur)
        if (sp >= 0) != (sc >= 0):
            t = sp / (sp - sc)
            out.append((prev[0] + t * (cur[0] - prev[0]), prev[1] + t * (cur[1] - prev[1])))
        if sc >= 0:
            out.append(cur)
    dedup = [p for i, p in enumerate(out) if abs(p[0] - out[i - 1][0]) > 1e-9 or abs(p[1] - out[i - 1][1]) > 1e-9]
    return dedup


def dpad_parts(outline):
    """Split the traced cross into four arms and the centre square, so each lights on its own.

    A partition (every point in exactly one part): the centre square |dx|, |dy| <= arm/2, and four
    wedges split by the diagonals (as the mockups decide a direction, make_mockups_cad.py
    dpad_dir), so the inner corner fillets are shared between neighbouring arms. Review
    phase1-chunk1 F3: clipping each arm on one axis only left slivers and spikes.
    """
    xs, ys = [p[0] for p in outline], [p[1] for p in outline]
    cx, cy, a = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2, DPAD_ARM / 2

    def wedge(ux, uy):
        # direction (ux, uy): beyond the centre square along it, and inside its 90-degree wedge
        poly = clip(outline, ux, uy, -(ux * cx + uy * cy) - a)
        for px, py in ((uy, ux), (-uy, -ux)):   # the two diagonals bounding the wedge
            poly = clip(poly, ux + px, uy + py, -((ux + px) * cx + (uy + py) * cy))
        return poly

    centre = outline
    for ux, uy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        centre = clip(centre, -ux, -uy, ux * cx + uy * cy + a)
    return {
        'dpad-up': wedge(0, 1),
        'dpad-down': wedge(0, -1),
        'dpad-left': wedge(-1, 0),
        'dpad-right': wedge(1, 0),
        'dpad-centre': centre,
    }


def top_y(poly, x):
    """Highest y where the polygon's outline crosses the vertical line at x."""
    ys = [y1 + (x - x1) * (y2 - y1) / (x2 - x1)
          for (x1, y1), (x2, y2) in zip(poly[-1:] + poly[:-1], poly)
          if x1 != x2 and (x1 - x) * (x2 - x) <= 0]
    return max(ys)


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
    oval_y = (top_y(t['body'], pn['cx']) + top_y(t['lens'], pn['cx'])) / 2
    add('top-slot', 'slot', pill(pn['cx'], oval_y, pn['length'], pn['height'], 0))
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
    def extra(pid, role):
        a = dict(SHADE.get(role, {}))
        if pid == 'dpad-centre':
            a = {}   # never lit and never on the cross's edge: no shade entries of its own (F7)
        if pid.startswith('dpad-'):
            a['data-group'] = 'dpad'   # the cross shades as one piece; each arm keeps its own entries
        return ''.join(f' {k}="{v}"' for k, v in a.items())

    out += [f'<path id="{pid}" class="{role}"{extra(pid, role)} d="{d(poly)}"/>' for pid, role, poly in parts]
    out.append('</svg>')
    return '\n'.join(out) + '\n', parts


if __name__ == '__main__':
    svg, parts = build()
    with open(OUT, 'w') as fh:
        fh.write(svg)
    print(f'wrote {os.path.relpath(OUT)} ({len(parts)} parts)')

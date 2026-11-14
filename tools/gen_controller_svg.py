#!/usr/bin/env python3
"""Generate assets/controller.svg (the AGB-001 front, drawn by us) from a table of dimensions.

    python3 tools/gen_controller_svg.py            # writes assets/controller.svg

Why a table: the outline must be our own drawing (see LICENSES.md). Dimensions are facts; this
script turns them into our own geometry. No third-party drawing is copied or traced.

Units: millimetres, x right from the left edge of the body, y down from the top edge.
Every value is PROVISIONAL until checked with callipers on a real GBA (tagged 'P' below):
the provisional numbers are the proportions of a reference drawing rescaled to the published
outer size (144.5 x 82.0 mm). When a value is measured, change its tag to 'M' and note it.

The SVG is the committed source of truth for the GBA assets (tools/svg_to_gba.py, next step). It
can be edited by hand (Inkscape keeps ids). Rerun this script only to regenerate from the table.

Part ids are stable and semantic (the asset pipeline keys palette entries off them):
  body, shoulder-l, shoulder-r, lens, lcd, top-slot, led, dpad-up, dpad-down, dpad-left,
  dpad-right, dpad-centre, btn-a, btn-b, start-plate, btn-start, select-plate, btn-select,
  speaker-1..N. No lettering or logos are drawn (owner decision, 2026-11-14).
"""
import math, os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'assets', 'controller.svg')

# ---- the dimension table (mm) --------------------------------------------------------------
# name: (value, tag)  P = provisional (reference proportions, rescaled), M = measured on a real GBA
D = {
    'width':            (144.5, 'P'),  # published AGB-001 outer width
    'height':           (82.0, 'P'),   # published AGB-001 outer height (centre of bottom edge)
    # Body silhouette, left half, mirrored about x = width/2. (x, y) control points of a smooth
    # closed curve, clockwise from the top centre: top edge -> shoulder slope -> left end -> bottom.
    'outline_top':      ([(72.25, 0.2), (43.5, 0.2), (29.0, 5.0), (14.5, 9.5), (7.0, 12.0), (3.0, 23.5)], 'P'),
    'outline_end':      ([(0.3, 35.5), (0.3, 50.0), (1.5, 64.0)], 'P'),
    'outline_bottom':   ([(5.5, 69.0), (14.5, 72.5), (29.0, 77.0), (43.5, 81.0), (58.0, 81.7), (72.25, 81.7)], 'P'),
    # Shoulder buttons: a band along the top-corner curve, only its raised edge shows
    'shoulder_l':       ((4.5, 33.0, 2.4, 0), 'P'),      # x0, x1, rise above the body edge, (unused)
    # Screen lens and LCD
    'lens':             ((34.2, 8.6, 110.3, 74.9), 'P'),     # x0, y0, x1, y1
    'lens_r_top':       (6.0, 'P'),
    'lens_r_bottom':    (12.0, 'P'),
    'lcd_size':         ((61.2, 40.8), 'P'),                 # published visible area of the AGB LCD
    'lcd_centre':       ((73.3, 37.3), 'P'),
    'top_slot':         ((72.25, 4.4, 16.5, 3.8), 'P'),      # centre x, centre y, length, height
    'led':              ((118.5, 14.4, 1.5), 'P'),           # centre x, y, radius
    # Controls
    'dpad':             ((20.2, 34.7, 16.9, 5.6), 'P'),      # centre x, y, tip-to-tip span, arm width
    'btn_a':            ((131.4, 31.5, 4.3), 'P'),           # centre x, y, radius
    'btn_b':            ((119.3, 35.9, 4.3), 'P'),
    'btn_start':        ((30.1, 54.6, 2.0), 'P'),            # the round button; START above SELECT
    'btn_select':       ((30.1, 62.9, 2.0), 'P'),
    'plate':            ((13.0, 3.2, 14.0), 'P'),            # length to the button centre, thickness, slope deg
    'speaker':          ((115.2, 51.0, 14.8, 1.6, 3.0, -14.0, 6), 'P'),  # x0, y0, length, width, pitch, slope deg, count
}


def v(name):
    return D[name][0]


# ---- geometry helpers ----------------------------------------------------------------------
def f(x):
    return f'{x:.2f}'.rstrip('0').rstrip('.')


def catmull_rom_closed(pts, steps=1):
    """Closed Catmull-Rom spline through pts, as an SVG cubic-Bezier path."""
    n = len(pts)
    d = [f'M{f(pts[0][0])},{f(pts[0][1])}']
    for i in range(n):
        p0, p1, p2, p3 = pts[i - 1], pts[i], pts[(i + 1) % n], pts[(i + 2) % n]
        c1 = (p1[0] + (p2[0] - p0[0]) / 6, p1[1] + (p2[1] - p0[1]) / 6)
        c2 = (p2[0] - (p3[0] - p1[0]) / 6, p2[1] - (p3[1] - p1[1]) / 6)
        d.append(f'C{f(c1[0])},{f(c1[1])} {f(c2[0])},{f(c2[1])} {f(p2[0])},{f(p2[1])}')
    return ' '.join(d) + ' Z'


def body_points():
    w = v('width')
    left = v('outline_top') + v('outline_end') + v('outline_bottom')
    # left half runs top-centre -> bottom-centre; mirror it for the right half (bottom -> top)
    right = [(w - x, y) for x, y in reversed(left[1:-1])]
    return left + right


def rounded_rect(x0, y0, x1, y1, rt, rb):
    return (f'M{f(x0 + rt)},{f(y0)} H{f(x1 - rt)} A{f(rt)},{f(rt)} 0 0 1 {f(x1)},{f(y0 + rt)} '
            f'V{f(y1 - rb)} A{f(rb)},{f(rb)} 0 0 1 {f(x1 - rb)},{f(y1)} H{f(x0 + rb)} '
            f'A{f(rb)},{f(rb)} 0 0 1 {f(x0)},{f(y1 - rb)} V{f(y0 + rt)} A{f(rt)},{f(rt)} 0 0 1 {f(x0 + rt)},{f(y0)} Z')


def slanted_pill(x0, y0, x1, y1, thick):
    """A capsule from (x0,y0) to (x1,y1)."""
    a = math.atan2(y1 - y0, x1 - x0)
    nx, ny, r = -math.sin(a) * thick / 2, math.cos(a) * thick / 2, thick / 2
    return (f'M{f(x0 + nx)},{f(y0 + ny)} L{f(x1 + nx)},{f(y1 + ny)} A{f(r)},{f(r)} 0 0 0 {f(x1 - nx)},{f(y1 - ny)} '
            f'L{f(x0 - nx)},{f(y0 - ny)} A{f(r)},{f(r)} 0 0 0 {f(x0 + nx)},{f(y0 + ny)} Z')


# ---- the drawing ---------------------------------------------------------------------------
def build():
    w, h = v('width'), v('height')
    parts = []   # (id, role, svg element)

    def path(pid, role, d):
        parts.append((pid, role, f'<path id="{pid}" class="{role}" d="{d}"/>'))

    def circle(pid, role, cx, cy, r):
        parts.append((pid, role, f'<circle id="{pid}" class="{role}" cx="{f(cx)}" cy="{f(cy)}" r="{f(r)}"/>'))

    # shoulders first: a band that follows the body's top-corner curve, raised by `rise`; the body
    # is drawn over its lower part, so only the sweep along the top corner shows.
    x0, x1, rise, _ = v('shoulder_l')
    top = sorted(v('outline_top') + v('outline_end')[:1], key=lambda p: p[0])
    curve = [(x, y) for x, y in top if x0 - 4 <= x <= x1 + 1]
    band = [(x, y - rise) for x, y in curve] + [(x, y + 8) for x, y in reversed(curve)]
    path('shoulder-l', 'shoulder', catmull_rom_closed(band))
    path('shoulder-r', 'shoulder', catmull_rom_closed([(w - x, y) for x, y in reversed(band)]))
    path('body', 'body', catmull_rom_closed(body_points()))
    lx0, ly0, lx1, ly1 = v('lens')
    path('lens', 'lens', rounded_rect(lx0, ly0, lx1, ly1, v('lens_r_top'), v('lens_r_bottom')))
    (lw, lh), (lcx, lcy) = v('lcd_size'), v('lcd_centre')
    parts.append(('lcd', 'lcd', f'<rect id="lcd" class="lcd" x="{f(lcx - lw / 2)}" y="{f(lcy - lh / 2)}" '
                                f'width="{f(lw)}" height="{f(lh)}"/>'))
    cx, cy, ln, ht = v('top_slot')
    path('top-slot', 'slot', slanted_pill(cx - ln / 2 + ht / 2, cy, cx + ln / 2 - ht / 2, cy, ht))
    circle('led', 'led', *v('led'))
    # d-pad: four arms + centre, each its own part (each lights independently)
    cx, cy, span, arm = v('dpad')
    s, a = span / 2, arm / 2
    for pid, (ax0, ay0, ax1, ay1) in {'dpad-up': (cx - a, cy - s, cx + a, cy - a),
                                      'dpad-down': (cx - a, cy + a, cx + a, cy + s),
                                      'dpad-left': (cx - s, cy - a, cx - a, cy + a),
                                      'dpad-right': (cx + a, cy - a, cx + s, cy + a)}.items():
        parts.append((pid, 'button', f'<rect id="{pid}" class="button" x="{f(ax0)}" y="{f(ay0)}" '
                                     f'width="{f(ax1 - ax0)}" height="{f(ay1 - ay0)}" rx="0.6"/>'))
    parts.append(('dpad-centre', 'button', f'<rect id="dpad-centre" class="button" x="{f(cx - a)}" y="{f(cy - a)}" '
                                           f'width="{f(arm)}" height="{f(arm)}"/>'))
    circle('btn-a', 'button', *v('btn_a'))
    circle('btn-b', 'button', *v('btn_b'))
    # start / select: a slanted label plate ending in the round button (START above SELECT)
    plen, pth, slope = v('plate')
    for name in ('start', 'select'):
        bx, by, br = v(f'btn_{name}')
        ang = math.radians(slope)
        px0, py0 = bx - plen * math.cos(ang), by - plen * math.sin(ang)
        path(f'{name}-plate', 'plate', slanted_pill(px0, py0, bx, by, pth))
        circle(f'btn-{name}', 'button', bx, by, br)
    # speaker grille
    sx0, sy0, sln, swd, pitch, sslope, count = v('speaker')
    ang = math.radians(sslope)
    for i in range(count):
        y = sy0 + i * pitch
        path(f'speaker-{i + 1}', 'slot', slanted_pill(sx0, y, sx0 + sln * math.cos(ang), y + sln * math.sin(ang), swd))

    provisional = sorted(k for k, (_, tag) in D.items() if tag == 'P')
    css = ('.body{fill:#5040a8}.shoulder{fill:#382c80}.lens{fill:#202030}.lcd{fill:#b0d890}'
           '.button{fill:#3c3c4c}.plate{fill:#3c3090}.slot{fill:#281c60}.led{fill:#58e070}')
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {f(w)} {f(h)}" width="{f(w)}mm" height="{f(h)}mm">',
           '<!-- wgc-gba-tx-ui: Game Boy Advance (AGB-001) front, our own drawing from a dimension table',
           '     (tools/gen_controller_svg.py). Units: mm. Part ids are used by the asset pipeline;',
           '     colours here are only a preview (the ROM assigns palette roles by class/id).',
           f'     Provisional dimensions (to be checked with callipers): {", ".join(provisional)} -->',
           f'<style>{css}</style>']
    out += [el for _, _, el in parts]
    out.append('</svg>')
    return '\n'.join(out) + '\n', parts


if __name__ == '__main__':
    svg, parts = build()
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    open(OUT, 'w').write(svg)
    print(f'wrote {os.path.relpath(OUT)} ({len(parts)} parts)')

"""Build a GBA-resolution label map of the real AGB-001 front from the project's CAD drawing.

Source: ../../Designs/dimensions/GBA_Front.dxf (177.1 x 98.4 mm front outline, mm units).
Output: mockups/gba_front_labels.png ('L' mode; pixel value = semantic part id, see PARTS)
        sized at SCALE px/mm, so the drawing is dimensionally true to the real console.

Needs: ezdxf, numpy, Pillow. E.g.  pip install --target <dir> ezdxf numpy; PYTHONPATH=<dir> python3 gba_front_labels.py
"""
import os, sys, json
from collections import deque
import ezdxf
import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
DXF = os.path.join(HERE, '..', '..', 'Designs', 'dimensions', 'GBA_Front.dxf')
OUT = os.path.join(HERE, '..', 'mockups', 'gba_front_labels.png')
K = 8.0           # hi-res px/mm for region finding
DOWN = 6          # hi-res -> target factor => target 1.333 px/mm (236 x 131 px)

PARTS = dict(outside=0, body=1, bevel=2, L=3, R=4, lens=5, lcd=6, dpad=7, A=8, B=9,
             start_plate=10, start=11, select_plate=12, select=13, speaker=14, led=15, topslot=16)

# Small gaps in the DXF outlines (found by open-endpoint search), closed so regions don't leak.
GAP_FIXES = [[(4.93, -24.54), (13.42, -24.55)],    # lens top
             [(-67.61, -52.58), (-62.35, -52.58)],  # d-pad bottom arm
             [(2.60, -112.79), (7.26, -112.79)]]    # body bottom edge
LCD_MM = (-33.53, -33.29, 43.39, -84.85)            # LCD rectangle (x0, y0, x1, y1)

# Interior seed points (mm) -> part. Found by labelling every enclosed region and matching to the render.
SEEDS = {'body': [(8.62, -23.99), (-45.0, -45.0)], 'lens': (-7.63, -68.61), 'dpad': (-59.01, -55.99),
         'A': (77.37, -52.11), 'B': (62.49, -57.49), 'L': (-63.13, -22.36), 'R': (72.62, -22.24),
         'start_plate': (-56.13, -77.99), 'start': (-46.88, -79.99),
         'select_plate': (-56.13, -87.86), 'select': (-46.88, -89.86),
         'led': (61.62, -31.49), 'topslot': (4.87, -19.61),
         'bevel': [(-81.51, -71.11), (91.49, -72.86)],
         'speaker': [(66.62, -75.61), (66.74, -79.11), (66.49, -82.86), (66.49, -86.49), (66.62, -90.11)]}


def load_paths():
    paths = []
    for e in ezdxf.readfile(DXF).modelspace():
        t = e.dxftype()
        if t == 'LINE': pts = [(e.dxf.start.x, e.dxf.start.y), (e.dxf.end.x, e.dxf.end.y)]
        elif t == 'SPLINE': pts = [(p.x, p.y) for p in e.construction_tool().flattening(0.05)]
        elif t == 'LWPOLYLINE': pts = [(p[0], p[1]) for p in e.get_points()]
        else: continue
        paths.append([(float(x), float(y)) for x, y in pts])
    return paths + GAP_FIXES


def main():
    paths = load_paths()
    xs = [x for p in paths for x, _ in p]; ys = [y for p in paths for _, y in p]
    minx, maxy = min(xs), max(ys)
    W = int((max(xs) - minx) * K) + 8; H = int((maxy - min(ys)) * K) + 8
    W += (-W) % DOWN; H += (-H) % DOWN
    px = lambda x, y: ((x - minx) * K + 4, (maxy - y) * K + 4)
    im = Image.new('L', (W, H), 255); d = ImageDraw.Draw(im)
    for p in paths: d.line([px(x, y) for x, y in p], fill=0, width=2)
    wall = np.array(im) == 0
    lab = np.full((H, W), 255, np.uint8)        # 255 = wall/unknown
    def fill(seed, val):
        sx, sy = map(int, px(*seed))
        if wall[sy, sx] or lab[sy, sx] != 255: return
        q = deque([(sy, sx)]); lab[sy, sx] = val
        while q:
            y, x = q.popleft()
            for yy, xx in ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)):
                if 0 <= yy < H and 0 <= xx < W and not wall[yy, xx] and lab[yy, xx] == 255:
                    lab[yy, xx] = val; q.append((yy, xx))
    fill((minx - 0.4, maxy + 0.4), PARTS['outside'])
    for name, s in SEEDS.items():
        for seed in (s if isinstance(s, list) else [s]): fill(seed, PARTS[name])
    # LCD rectangle overrides lens (its own grid lines are not closed regions)
    x0, y0 = px(LCD_MM[0], LCD_MM[1]); x1, y1 = px(LCD_MM[2], LCD_MM[3])
    lab[int(y0):int(y1), int(x0):int(x1)] = PARTS['lcd']
    # anything still unknown (walls, leftover slivers) -> mode of the block in downsampling
    h, w = H // DOWN, W // DOWN
    blocks = lab.reshape(h, DOWN, w, DOWN).transpose(0, 2, 1, 3).reshape(h, w, DOWN * DOWN)
    out = np.zeros((h, w), np.uint8)
    for j in range(h):
        for i in range(w):
            v = blocks[j, i]; v = v[v != 255]
            out[j, i] = np.bincount(v).argmax() if len(v) else PARTS['body']
    Image.fromarray(out, 'L').save(OUT)
    json.dump(PARTS, open(OUT.replace('.png', '.json'), 'w'), indent=1)
    print('wrote', OUT, out.shape, 'px/mm', K / DOWN)


if __name__ == '__main__':
    main()

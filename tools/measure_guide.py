#!/usr/bin/env python3
"""Draw build/measure_guide.svg/.png: assets/controller.svg with the numbered markers of MEASURE.md.

    python3 tools/measure_guide.py      (needs rsvg-convert for the PNG)
"""
import os, re, subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'assets', 'controller.svg')
OUT = os.path.join(ROOT, 'build', 'measure_guide')

# number -> (x, y) of the label in mm (same coordinates as the drawing)
MARKS = {1: (72.25, 85.5), 2: (95, 80), 3: (15, 69), 4: (72.25, 66), 5: (36, 11), 6: (9, 34.7),
         7: (20.2, 34.7), 8: (131.4, 24), 9: (119.3, 44), 10: (35, 52), 11: (35, 66), 12: (14, 58),
         13: (124, 14.4), 14: (122, 70), 15: (72.25, 1.8), 16: (22, 3), 17: (10, 1), 18: (73.3, 37.3)}


def main():
    svg = open(SRC).read()
    w, h = map(float, re.search(r'viewBox="0 0 ([\d.]+) ([\d.]+)"', svg).groups())
    pad = 6
    svg = svg.replace(f'viewBox="0 0 {w:g} {h:g}"', f'viewBox="-2 -4 {w + 4:g} {h + pad + 4:g}"', 1)
    marks = []
    for n, (x, y) in MARKS.items():
        marks.append(f'<circle cx="{x}" cy="{y}" r="2.6" fill="#f8c048" stroke="#101828" stroke-width="0.4"/>'
                     f'<text x="{x}" y="{y + 1.2}" font-size="3.4" font-family="sans-serif" font-weight="bold" '
                     f'text-anchor="middle" fill="#101828">{n}</text>')
    svg = svg.replace('</svg>', '\n'.join(marks) + '\n</svg>')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    open(OUT + '.svg', 'w').write(svg)
    subprocess.run(['rsvg-convert', '-w', '900', '-b', '#101828', OUT + '.svg', '-o', OUT + '.png'], check=True)
    print('wrote', os.path.relpath(OUT + '.png', ROOT))


if __name__ == '__main__':
    main()

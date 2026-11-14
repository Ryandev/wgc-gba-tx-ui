#!/usr/bin/env python3
"""Turn assets/controller.svg into GBA graphics: an 8bpp affine background, one palette entry per part.

    python3 tools/svg_to_gba.py SVG OUTDIR [--width 216] [--name ctrl]

Writes OUTDIR/<name>.c, OUTDIR/<name>.h and OUTDIR/<name>_preview.png.

How: every shape element (path, rect, circle, ellipse, polygon, polyline, line) with an id is a
"part", in document order, at any depth (Inkscape layers/groups are fine; attribute order doesn't
matter). Its palette role is its own class, else the nearest ancestor's. Each part is rendered on its own (rsvg-convert) as a
white-on-black coverage mask at the target size; a pixel belongs to a part when the part covers at
least half of it, and later parts in the file are on top (SVG painter's order). Pixel value =
palette index of the topmost part (0 = no part, transparent). So the picture has hard edges and
lighting a button at run time is one palette write (SPEC.md: buttons lit by palette writes in VBlank).

The image is cut into 8x8 tiles (64 bytes each at 8bpp), identical tiles are stored once, and the
tile map is a 32x32 affine map (one byte per entry, 256x256 px; the picture sits at its top left).
Affine maps can't flip tiles and index at most 256 tiles; the script fails if the picture needs more.
The palette's default colours are the SVG's preview colours (its <style> classes); the ROM may
replace them (colour presets are a stretch goal). Output is deterministic.
Needs rsvg-convert (librsvg2-bin) and Pillow.
"""
import argparse
import io
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

from PIL import Image

TILE = 8
MAP_TILES = 32            # affine map 32x32 tiles = 256x256 px (BG_AFF_32x32)
MAX_TILES = 256           # affine map entries are one byte
SHAPES = {'path', 'rect', 'circle', 'ellipse', 'polygon', 'polyline', 'line'}
ID_RE = re.compile(r'^[A-Za-z][\w-]*$')   # ids we can name in CSS and in a C enum


def parts_of(svg):
    """[(id, class)] of the shape elements that have an id, in document order."""
    out = []

    def walk(el, inherited):
        cls = el.get('class') or inherited
        tag = el.tag.rsplit('}', 1)[-1]
        if tag in SHAPES and el.get('id'):
            if not ID_RE.match(el.get('id')):
                raise ValueError(f'part id {el.get("id")!r}: use letters, digits, - and _ only')
            out.append((el.get('id'), cls))
        for child in el:
            walk(child, cls)

    walk(ET.fromstring(svg), '')
    ids = [i for i, _ in out]
    dup = {i for i in ids if ids.count(i) > 1}
    if dup:
        raise ValueError(f'duplicate part ids: {sorted(dup)}')
    return out


def class_colours(svg):
    """{class: (r, g, b)} from the SVG's <style> (simple '.cls{fill:#rrggbb}' rules)."""
    out = {}
    style = re.search(r'<style>(.*?)</style>', svg, re.S)
    for cls, hexrgb in re.findall(r'\.([\w-]+)\s*\{\s*fill:\s*#([0-9a-fA-F]{6})', style.group(1) if style else ''):
        out[cls] = tuple(int(hexrgb[i:i + 2], 16) for i in (0, 2, 4))
    return out


def svg_size(svg):
    w, h = map(float, re.search(r'viewBox="0 0 ([\d.]+) ([\d.]+)"', svg).groups())
    return w, h


def render_mask(svg, part_id, width_px, height_px):
    """Coverage of one part as a list of rows of 0..255, at exactly width_px x height_px."""
    # visibility (unlike display) can be switched back on below a hidden ancestor, so this works
    # inside groups; !important beats class rules on the part and its children.
    only = (f'<style>* {{ visibility: hidden !important }} '
            f'#{part_id}, #{part_id} * {{ visibility: visible !important; fill: #fff !important; '
            f'stroke: none !important; opacity: 1 !important }}</style></svg>')
    doc = svg.replace('</svg>', only, 1)
    png = subprocess.run(['rsvg-convert', '-w', str(width_px), '-h', str(height_px), '-b', '#000'],
                         input=doc.encode(), capture_output=True, check=True).stdout
    img = Image.open(io.BytesIO(png)).convert('L')
    if img.size != (width_px, height_px):
        raise ValueError(f'{part_id}: rendered {img.size}, wanted {(width_px, height_px)}')
    return img


def label_map(svg, width_px):
    vw, vh = svg_size(svg)
    height_px = round(width_px * vh / vw)
    parts = parts_of(svg)
    if len(parts) > 255:
        raise ValueError(f'{len(parts)} parts: an 8bpp palette has room for 255')
    labels = Image.new('L', (width_px, height_px), 0)
    for i, (pid, _) in enumerate(parts, 1):
        mask = render_mask(svg, pid, width_px, height_px).point(lambda v: 255 if v >= 128 else 0)
        if not mask.getbbox():
            raise ValueError(f'part {pid!r} covers no pixel at {width_px} px wide')
        labels.paste(i, (0, 0), mask)
    return labels, parts


def tiles_and_map(labels):
    w, h = labels.size
    tw, th = -(-w // TILE), -(-h // TILE)
    if tw > MAP_TILES or th > MAP_TILES:
        raise ValueError(f'{w}x{h} px does not fit a 256x256 affine map')
    px = labels.load()
    tiles, index, amap = [bytes(TILE * TILE)], {bytes(TILE * TILE): 0}, [0] * (MAP_TILES * MAP_TILES)
    for ty in range(th):
        for tx in range(tw):
            t = bytes(px[tx * TILE + x, ty * TILE + y] if tx * TILE + x < w and ty * TILE + y < h else 0
                      for y in range(TILE) for x in range(TILE))
            if t not in index:
                index[t] = len(tiles)
                tiles.append(t)
            amap[ty * MAP_TILES + tx] = index[t]
    if len(tiles) > MAX_TILES:
        raise ValueError(f'{len(tiles)} unique tiles; an affine background indexes at most {MAX_TILES}')
    return tiles, amap


def rgb15(rgb):
    r, g, b = (c >> 3 for c in rgb)
    return r | g << 5 | b << 10


def c_ident(s):
    return re.sub(r'\W', '_', s).upper()


def emit(name, svg_path, labels, parts, colours, tiles, amap):
    w, h = labels.size
    pal = [0] + [rgb15(colours.get(cls, (255, 0, 255))) for _, cls in parts]
    used = len(pal)
    pal += [0] * (len(pal) % 2)   # whole words, so a 32-bit copy of the array is safe
    up = name.upper()
    hdr = [f'// Generated by tools/svg_to_gba.py from {os.path.basename(svg_path)}: do not edit.',
           f'#ifndef GEN_{up}_H', f'#define GEN_{up}_H', '#include <tonc.h>', '',
           f'#define {up}_W {w}           // picture size in px (top left of the 256x256 affine map)',
           f'#define {up}_H {h}',
           f'#define {up}_TILES {len(tiles)}      // 8bpp tiles (64 bytes each)',
           f'#define {up}_PAL_LEN {used}     // palette entries used: 0 = transparent, then one per part', '',
           '// Palette index of each part (light a part by writing its entry)', f'enum {name}_part {{']
    hdr += [f'    {up}_{c_ident(pid)} = {i},' for i, (pid, _) in enumerate(parts, 1)]
    hdr += ['};', '',
            f'extern const u32 {name}_tiles[{len(tiles) * 16}];   // copy to a charblock',
            f'extern const u32 {name}_map[{MAP_TILES * MAP_TILES // 4}];     // 32x32 bytes; copy to a screenblock',
            f'extern const u16 {name}_pal[{len(pal)}];     // default (preview) colours; padded to whole words', '', '#endif', '']

    def words(data):
        return [int.from_bytes(data[i:i + 4], 'little') for i in range(0, len(data), 4)]

    def block(ty, sym, vals, fmt, per):
        out = [f'const {ty} {sym}[{len(vals)}] __attribute__((aligned(4))) = {{']
        out += ['    ' + ', '.join(fmt.format(v) for v in vals[i:i + per]) + ',' for i in range(0, len(vals), per)]
        return out + ['};', '']

    src = [f'// Generated by tools/svg_to_gba.py from {os.path.basename(svg_path)}: do not edit.',
           f'#include "{name}.h"', '']
    src += block('u32', f'{name}_tiles', words(b''.join(tiles)), '0x{:08X}', 8)
    src += block('u32', f'{name}_map', words(bytes(amap)), '0x{:08X}', 8)
    src += block('u16', f'{name}_pal', pal, '0x{:04X}', 8)
    return '\n'.join(hdr), '\n'.join(src), pal


def preview(labels, pal):
    img = labels.convert('P')
    flat = []
    for c in pal:
        flat += [(c & 31) << 3, (c >> 5 & 31) << 3, (c >> 10 & 31) << 3]
    flat[0:3] = [16, 24, 40]
    img.putpalette(flat + [0] * (768 - len(flat)))
    return img.convert('RGB').resize((labels.width * 3, labels.height * 3), Image.NEAREST)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('svg')
    ap.add_argument('outdir')
    ap.add_argument('--width', type=int, default=216)
    ap.add_argument('--name', default='ctrl')
    a = ap.parse_args(argv)
    svg = open(a.svg).read()
    try:
        labels, parts = label_map(svg, a.width)
        tiles, amap = tiles_and_map(labels)
    except (ValueError, subprocess.CalledProcessError) as e:
        print(f'svg_to_gba: {e}', file=sys.stderr)
        return 1
    hdr, src, pal = emit(a.name, a.svg, labels, parts, class_colours(svg), tiles, amap)
    os.makedirs(a.outdir, exist_ok=True)
    for ext, text in (('h', hdr), ('c', src)):
        with open(os.path.join(a.outdir, f'{a.name}.{ext}'), 'w') as f:
            f.write(text)
    preview(labels, pal).save(os.path.join(a.outdir, f'{a.name}_preview.png'))
    print(f'svg_to_gba: {labels.width}x{labels.height} px, {len(parts)} parts, {len(tiles)} tiles')
    return 0


if __name__ == '__main__':
    sys.exit(main())

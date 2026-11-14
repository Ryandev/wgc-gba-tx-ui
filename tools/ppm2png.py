"""Convert harness PPM screenshots to PNG (nearest-neighbour scale) and build a labelled contact sheet.
Usage: python3 ppm2png.py <scale> <out_dir> <ppm>...   (no third-party deps)"""
import sys, zlib, struct, os

def read_ppm(p):
    d = open(p, 'rb').read()
    parts = d.split(b'\n', 3)
    w, h = map(int, parts[1].split())
    return w, h, parts[3]

def write_png(path, w, h, rgb):
    raw = b''.join(b'\x00' + rgb[y*w*3:(y+1)*w*3] for y in range(h))
    def chunk(t, b): return struct.pack('>I', len(b)) + t + b + struct.pack('>I', zlib.crc32(t + b) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
                           + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))

def scale(w, h, rgb, s):
    out = bytearray()
    for y in range(h):
        row = bytearray()
        for x in range(w):
            row += rgb[(y*w+x)*3:(y*w+x)*3+3] * s
        out += bytes(row) * s
    return w*s, h*s, bytes(out)

s = int(sys.argv[1]); od = sys.argv[2]
for p in sys.argv[3:]:
    w, h, rgb = read_ppm(p)
    name = os.path.basename(os.path.dirname(p)) + '__' + os.path.basename(p)[:-4] + '.png'
    W, H, big = scale(w, h, rgb, s)
    write_png(os.path.join(od, name), W, H, big)
    print(name)

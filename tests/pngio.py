"""Minimal PNG read/write for 8-bit RGB images (no third-party dependencies).

write_png() emits filter-0 scanlines. read_png() accepts any standard 8-bit RGB/RGBA
non-interlaced PNG (all five filter types), so goldens edited elsewhere still load.
"""
import struct, zlib


def write_png(path, w, h, rgb):
    raw = b''.join(b'\x00' + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def chunk(t, b):
        return struct.pack('>I', len(b)) + t + b + struct.pack('>I', zlib.crc32(t + b) & 0xffffffff)

    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
                + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


def read_png(path):
    """-> (w, h, rgb bytes)"""
    data = open(path, 'rb').read()
    if data[:8] != b'\x89PNG\r\n\x1a\n': raise ValueError(f'{path}: not a PNG')
    pos, idat, w, ended = 8, b'', None, False
    while pos < len(data):
        n, t = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        crc = struct.unpack('>I', data[pos + 8 + n:pos + 12 + n])[0]
        if crc != zlib.crc32(t + body) & 0xffffffff: raise ValueError(f'{path}: bad CRC in {t!r}')
        if t == b'IEND': ended = True
        if t == b'IHDR':
            w, h, depth, ctype, _, _, interlace = struct.unpack('>IIBBBBB', body)
            if depth != 8 or ctype not in (2, 6) or interlace: raise ValueError(f'{path}: unsupported PNG')
            bpp = 3 if ctype == 2 else 4
        elif t == b'IDAT':
            idat += body
        pos += 12 + n
    if w is None or not ended: raise ValueError(f'{path}: missing IHDR or IEND')
    raw, out, prev, stride = zlib.decompress(idat), bytearray(), bytearray(w * bpp), w * bpp
    if len(raw) != h * (stride + 1): raise ValueError(f'{path}: image data length mismatch')
    for y in range(h):
        ft, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        if ft > 4: raise ValueError(f'{path}: invalid filter {ft} on row {y}')
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b, c = prev[x], prev[x - bpp] if x >= bpp else 0
            if ft == 1: line[x] = (line[x] + a) & 255
            elif ft == 2: line[x] = (line[x] + b) & 255
            elif ft == 3: line[x] = (line[x] + (a + b) // 2) & 255
            elif ft == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        prev = line
        if bpp == 4 and any(line[i] != 255 for i in range(3, stride, 4)):
            raise ValueError(f'{path}: transparent pixels (goldens must be opaque)')
        out += line if bpp == 3 else bytes(v for i, v in enumerate(line) if i % 4 != 3)
    return w, h, bytes(out)


def read_ppm(path):
    d = open(path, 'rb').read()
    parts = d.split(b'\n', 3)  # the harness writes exactly "P6\n<w> <h>\n255\n<rgb>"
    if len(parts) != 4 or parts[0] != b'P6' or parts[2] != b'255': raise ValueError(f'{path}: not a harness PPM')
    w, h = map(int, parts[1].split())
    if len(parts[3]) != w * h * 3: raise ValueError(f'{path}: PPM payload length mismatch')
    return w, h, parts[3]

#!/usr/bin/env python3
"""Tests for the asset generators in tools/ (run by `make tools-test`, part of `make check`)."""
import os
import random
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gen_controller_svg  # noqa: E402
import svg_to_gba  # noqa: E402
import ttf_to_tfont  # noqa: E402

def read(path):
    with open(path) as f:
        return f.read()


FONT = os.path.join(ROOT, 'assets', 'fonts', 'PressStart2P-Regular.ttf')


def bitmap(rows):
    return [''.join('#' if r >> x & 1 else '.' for x in range(8)) for r in rows]


class TtfToTfont(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.glyphs = ttf_to_tfont.build(FONT, 32, 96)

    def test_count_and_space(self):
        self.assertEqual(len(self.glyphs), 96)
        self.assertEqual(self.glyphs[0], [0] * 8)  # ' ' is blank

    def test_letter_a_lsb_is_leftmost(self):
        self.assertEqual(bitmap(self.glyphs[ord('A') - 32]), [
            '..###...', '.##.##..', '##...##.', '##...##.',
            '#######.', '##...##.', '##...##.', '........'])

    def test_every_printable_glyph_has_ink(self):
        for code in range(33, 127):
            self.assertTrue(any(self.glyphs[code - 32]), chr(code))

    def test_c_output_is_deterministic_and_complete(self):
        with tempfile.TemporaryDirectory() as d:
            outs = []
            for n in (1, 2):
                p = os.path.join(d, f'f{n}.c')
                self.assertEqual(ttf_to_tfont.main([FONT, 'testFont', p]), 0)
                outs.append(read(p))
        self.assertEqual(outs[0], outs[1])
        c = outs[0]
        self.assertIn('static const u32 testFont_glyphs[192]', c)
        self.assertIn('.charOffset = 32,', c)
        self.assertIn('.bpp = 1,', c)
        self.assertIn('Press Start 2P (SIL Open Font License 1.1', c)

    def test_word_packing_matches_libtonc(self):
        # Rows 0..3 of a glyph go into one little-endian word, row 0 in the low byte (as sys8Font).
        rows = [1, 2, 3, 4, 5, 6, 7, 8]
        c = ttf_to_tfont.to_c([rows], 'x', 32, FONT)
        self.assertIn('0x04030201, 0x08070605', c)

    def test_rejects_a_font_off_the_grid(self):
        # Rendering Press Start 2P at 8 px but offset by half a pixel forces grey pixels.
        from PIL import ImageFont
        font = ImageFont.truetype(FONT, 12)  # 12 px: 1.5 px per font pixel
        with self.assertRaises(ValueError):
            ttf_to_tfont.glyph_rows(font, 'A')


# 32x16 units rendered at 32 px wide = 1 px per unit, so edges fall exactly on pixel boundaries.
SVG = """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 16" width="32mm" height="16mm">
<style>.a{fill:#ff0000}.b{fill:#00ff08}</style>
<rect id="back" class="a" x="0" y="0" width="16" height="16"/>
<rect id="front" class="b" x="8" y="0" width="8" height="8"/>
</svg>"""


class SvgToGba(unittest.TestCase):
    def test_parts_painters_order_and_tiles(self):
        labels, parts = svg_to_gba.label_map(SVG, 32)
        self.assertEqual(parts, [('back', 'a'), ('front', 'b')])
        self.assertEqual(labels.size, (32, 16))
        px = labels.load()
        self.assertEqual((px[0, 0], px[8, 0], px[15, 7], px[8, 8], px[16, 0]), (1, 2, 2, 1, 0))
        tiles, amap = svg_to_gba.tiles_and_map(labels)
        # blank, all-'back', all-'front' (the two lower 'back' tiles dedupe with the first)
        self.assertEqual(len(tiles), 3)
        self.assertEqual(amap[:4], [1, 2, 0, 0])
        self.assertEqual(amap[32:36], [1, 1, 0, 0])
        self.assertEqual(len(amap), 1024)

    def test_palette_is_bgr555_from_css_classes(self):
        self.assertEqual(svg_to_gba.class_colours(SVG), {'a': (255, 0, 0), 'b': (0, 255, 8)})
        self.assertEqual(svg_to_gba.rgb15((255, 0, 0)), 0x001F)
        self.assertEqual(svg_to_gba.rgb15((0, 255, 8)), 31 << 5 | 1 << 10)  # = 0x07E0

    def test_c_output_deterministic(self):
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, 't.svg')
            with open(p, 'w') as f:
                f.write(SVG)
            outs = []
            for n in (1, 2):
                o = os.path.join(d, str(n))
                self.assertEqual(svg_to_gba.main([p, o, '--width', '32', '--name', 'tst']), 0)
                outs.append(read(os.path.join(o, 'tst.c')) + read(os.path.join(o, 'tst.h')))
        self.assertEqual(outs[0], outs[1])
        self.assertIn('TST_FRONT = 2,', outs[0])
        self.assertIn('const u16 tst_pal[4]', outs[0])   # 3 used, padded to whole words
        self.assertIn('#define TST_PAL_LEN 3', outs[0])
        self.assertIn('0x0000, 0x001F, 0x07E0', outs[0])

    def test_rejects_too_many_tiles_and_invisible_parts(self):
        from PIL import Image
        noisy = Image.new('L', (256, 256))
        rng = random.Random(1)  # 1024 distinct random tiles
        noisy.putdata([rng.randrange(256) for _ in range(256 * 256)])
        with self.assertRaises(ValueError):
            svg_to_gba.tiles_and_map(noisy)
        tiny = SVG.replace('</svg>', '<rect id="dot" class="a" x="1" y="1" width="0.1" height="0.1"/></svg>')
        with self.assertRaises(ValueError):
            svg_to_gba.label_map(tiny, 32)

    def test_tile_pixel_order_and_word_packing(self):
        # One pixel lit at (x=1, y=0) and one at (x=0, y=2) inside tile 0: row-major, little endian.
        from PIL import Image
        img = Image.new('L', (8, 8), 0)
        img.putpixel((1, 0), 5)
        img.putpixel((0, 2), 7)
        tiles, amap = svg_to_gba.tiles_and_map(img)
        self.assertEqual(amap[0], 1)
        self.assertEqual(tiles[1][1], 5)
        self.assertEqual(tiles[1][16], 7)
        hdr, src, _ = svg_to_gba.emit('t', 'x.svg', img, [('p', (0, 0, 0))], tiles, amap)
        # tile 1 starts at word 16: bytes 0..3 = 00 05 00 00 -> 0x00000500; bytes 16..19 -> word 20 = 0x00000007
        words = src.split('t_tiles[32]')[1].split('};')[0].replace('\n', ' ').split(',')
        words = [w.strip(' ={') for w in words if w.strip(' ={')]
        self.assertEqual(words[16], '0x00000500')
        self.assertEqual(words[20], '0x00000007')
        # map: entry 0 = tile 1 in the low byte of the first word
        self.assertIn('t_map[256] __attribute__((aligned(4))) = {\n    0x00000001,', src)

    def test_nested_groups_any_attribute_order(self):
        svg = ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 16"><style>.a{fill:#200000}</style>'
               '<g id="layer1" class="a"><rect x="0" y="0" width="16" height="16" id="back"/>'
               '<g><rect class="b" style="fill:#000" id="front" x="8" y="0" width="8" height="8"/></g></g></svg>')
        labels, parts = svg_to_gba.label_map(svg, 32)
        self.assertEqual(parts, [('back', 'a'), ('front', 'b')])   # class inherited from the group
        px = labels.load()
        self.assertEqual((px[0, 0], px[8, 0], px[8, 8], px[20, 0]), (1, 2, 1, 0))

    def test_rejects_duplicate_or_unsafe_ids(self):
        with self.assertRaises(ValueError):
            svg_to_gba.parts_of(SVG.replace('id="front"', 'id="back"'))
        with self.assertRaises(ValueError):
            svg_to_gba.parts_of(SVG.replace('id="front"', 'id="front:1"'))

    def test_committed_svg_matches_its_generator(self):
        svg, _ = gen_controller_svg.build()
        self.assertEqual(svg, read(os.path.join(ROOT, 'assets', 'controller.svg')),
                         'assets/controller.svg is stale: run python3 tools/gen_controller_svg.py')

    def test_dpad_split_into_arms_and_centre(self):
        # a plus sign: arms 5.75 wide, 17.25 tip to tip, centred on (0, 0)
        a, s = gen_controller_svg.DPAD_ARM / 2, 17.25 / 2
        cross = [(-a, s), (a, s), (a, a), (s, a), (s, -a), (a, -a), (a, -s), (-a, -s), (-a, -a), (-s, -a), (-s, a), (-a, a)]
        parts = gen_controller_svg.dpad_parts(cross)

        def area(p):
            return abs(sum(p[i][0] * p[i - 1][1] - p[i - 1][0] * p[i][1] for i in range(len(p)))) / 2

        arm = (s - a) * 2 * a
        for pid in ('dpad-up', 'dpad-down', 'dpad-left', 'dpad-right'):
            self.assertAlmostEqual(area(parts[pid]), arm, places=6, msg=pid)
        self.assertAlmostEqual(area(parts['dpad-centre']), (2 * a) ** 2, places=6)
        self.assertGreater(min(y for _, y in parts['dpad-up']), 0)    # up is +y in the CAD frame
        self.assertLess(max(x for x, _ in parts['dpad-left']), 0)

    def test_top_oval_centred_between_shell_and_lens(self):
        _, parts = gen_controller_svg.build()
        poly = {pid: p for pid, _, p in parts}
        ys = [y for _, y in poly['top-slot']]
        cx = gen_controller_svg.SHELL_POCKETS['panel']['cx']
        gap_mid = (gen_controller_svg.top_y(poly['body'], cx) + gen_controller_svg.top_y(poly['lens'], cx)) / 2
        self.assertAlmostEqual((min(ys) + max(ys)) / 2, gap_mid, places=6)

    def test_pill_size_and_angle(self):
        p = gen_controller_svg.pill(10, 5, 16, 2, 0)
        xs, ys = [x for x, _ in p], [y for _, y in p]
        self.assertAlmostEqual(min(xs), 2)
        self.assertAlmostEqual(max(xs), 18)
        self.assertAlmostEqual(max(ys) - min(ys), 2, places=6)
        q = gen_controller_svg.pill(0, 0, 16, 2, 90)
        self.assertAlmostEqual(max(y for _, y in q), 8)

    def test_shading_raised_rim_tophi_glow(self):
        from PIL import Image
        # body 1 fills 12x12 at (2,2); a raised 4x4 button 2 at (5,5); lens 3 (top-hi) 2x2 at (10,10);
        # LED 4 at (9,4) glowing into body
        img = Image.new('L', (16, 16), 0)
        img.paste(1, (2, 2, 14, 14))
        img.paste(2, (5, 5, 9, 9))
        img.paste(3, (10, 10, 12, 12))
        img.putpixel((9, 4), 4)
        parts = [('body', 'b'), ('btn', 'k'), ('lens', 'l'), ('led', 'g')]
        attrs = {'body': {'data-shade': 'rim', 'data-rim': '#010101'},
                 'btn': {'data-shade': 'raised', 'data-hi': '#020202', 'data-lo': '#030303'},
                 'lens': {'data-shade': 'top-hi', 'data-hi': '#040404'},
                 'led': {'data-glow-into': 'body'}}
        colours = {'b': (100, 100, 100), 'k': (0, 0, 0), 'l': (0, 0, 0), 'g': (200, 200, 200)}
        out, entries = svg_to_gba.shade(img, parts, attrs, colours)
        names = [n for n, _ in entries]
        self.assertEqual(names, ['body', 'btn', 'lens', 'led', 'body-rim', 'btn-hi', 'btn-lo', 'lens-hi',
                                 'led-glow1', 'led-glow2'])
        e = {n: i for i, n in enumerate(names, 1)}
        px = out.load()
        self.assertEqual(px[2, 7], e['body-rim'])      # left edge of the body
        self.assertEqual(px[7, 13], e['body-rim'])     # bottom edge
        self.assertEqual(px[13, 7], e['body'])         # right edge: no rim (as in the mockups)
        self.assertEqual(px[6, 5], e['btn-hi'])        # top row of the button
        self.assertEqual(px[5, 7], e['btn-hi'])        # left column
        self.assertEqual(px[7, 8], e['btn-lo'])        # bottom row
        self.assertEqual(px[7, 7], e['btn'])           # inside
        self.assertEqual(px[11, 10], e['lens-hi'])     # top row of the lens (body above it)
        self.assertEqual(px[11, 11], e['lens'])
        self.assertEqual(px[9, 4], e['led'])
        self.assertEqual(px[10, 4], e['led-glow1'])    # d2 = 1
        self.assertEqual(px[11, 5], e['led-glow1'])    # d2 = 5 (<= 6)
        self.assertEqual(px[11, 6], e['led-glow2'])    # d2 = 8 (<= 12)
        self.assertEqual(entries[e['led-glow1'] - 1][1], (155, 155, 155))   # body + 0.55 of the way to LED

    def test_shading_group_has_no_inner_edges(self):
        from PIL import Image
        img = Image.new('L', (10, 6), 0)
        img.paste(1, (1, 1, 5, 5))
        img.paste(2, (5, 1, 9, 5))
        raised = {'data-shade': 'raised', 'data-hi': '#ffffff', 'data-lo': '#000000', 'data-group': 'g'}
        out, entries = svg_to_gba.shade(img, [('a', 'x'), ('b', 'x')], {'a': raised, 'b': raised}, {})
        e = {n: i for i, (n, _) in enumerate(entries, 1)}
        self.assertEqual(out.getpixel((5, 2)), e['b'])   # left neighbour is 'a', same group: no highlight
        self.assertEqual(out.getpixel((1, 2)), e['a-hi'])

    def test_real_drawing_fits(self):
        svg = read(os.path.join(ROOT, 'assets', 'controller.svg'))
        labels, parts = svg_to_gba.label_map(svg, 232)
        tiles, _ = svg_to_gba.tiles_and_map(labels)
        self.assertLessEqual(len(tiles), svg_to_gba.MAX_TILES)
        self.assertEqual(len(parts), 25)
        # every part is visible (none fully hidden under a later one)
        self.assertEqual({i for i, n in enumerate(labels.histogram()) if n}, set(range(len(parts) + 1)))


if __name__ == '__main__':
    unittest.main()

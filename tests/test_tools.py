#!/usr/bin/env python3
"""Tests for the asset generators in tools/ (run by `make tools-test`, part of `make check`)."""
import os
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import ttf_to_tfont  # noqa: E402

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
                outs.append(open(p).read())
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


if __name__ == '__main__':
    unittest.main()

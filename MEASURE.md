# Measuring a real GBA (AGB-001) for `assets/controller.svg`

The controller drawing comes from a dimension table in `tools/gen_controller_svg.py`. Every value is
**provisional** (tag `P`): proportions of a reference drawing rescaled to the published outer size.
Measuring the console turns them into first-hand values (tag `M`).

**How:** callipers, on the front face. **x** = from the console's **left** edge to the right,
**y** = from the **top** edge (top of the case, *not* counting the L/R buttons) downwards. Centres
are the middle of a round feature. Write the measured value in the last column; I'll update the
table, regenerate the SVG and show you the before/after. Numbers refer to the guide image
(`build/measure_guide.png`, made by `python3 tools/measure_guide.py`).

Any subset helps. **The first six matter most:** they fix the overall look.

| # | What | How | Provisional (mm) | Measured |
|---|---|---|---|---|
| 1 | Overall width | widest point, left grip to right grip | 144.5 (published) | |
| 2 | Overall height | top edge to the lowest point of the bottom edge (middle) | 82.0 (published) | |
| 3 | Bottom edge at the grips | top edge to the bottom edge, measured 15 mm in from the left end | 72.5 | |
| 4 | Screen lens, size | width × height of the dark lens window (plastic, not the LCD) | 76.1 × 66.3 | |
| 5 | Screen lens, position | lens left edge (x) and top edge (y) | x 34.2, y 8.6 | |
| 6 | D-pad, size | tip-to-tip width; width of one arm | 16.9; 5.6 | |
| 7 | D-pad, centre | centre of the cross | x 20.2, y 34.7 | |
| 8 | A button | diameter; centre | ⌀ 8.6; x 131.4, y 31.5 | |
| 9 | B button | diameter; centre | ⌀ 8.6; x 119.3, y 35.9 | |
| 10 | START button (upper) | diameter of the round button; centre | ⌀ 4.0; x 30.1, y 54.6 | |
| 11 | SELECT button (lower) | diameter; centre | ⌀ 4.0; x 30.1, y 62.9 | |
| 12 | START/SELECT plate | length from the plate's left end to the button centre; plate thickness; slope (rough angle) | 13.0; 3.2; 14° | |
| 13 | Power LED | centre; diameter | x 118.5, y 14.4; ⌀ 3.0 | |
| 14 | Speaker slots | number of slots; slot length; distance between slots; first slot's left end (x, y) | 6; 14.8; 3.0; x 115.2, y 51.0 | |
| 15 | Top slot (above the screen) | centre; length × height | x 72.3, y 4.4; 16.5 × 3.8 | |
| 16 | Top edge slope | top edge height (y) measured 15 mm and 30 mm in from the left end | 9.5; 5.0 | |
| 17 | Shoulder buttons | where the L button starts and ends along the top edge (x) | x 4.5 to 33.0 | |
| 18 | LCD (visible picture) | width × height (published 61.2 × 40.8) and its centre | 61.2 × 40.8; x 73.3, y 37.3 | |

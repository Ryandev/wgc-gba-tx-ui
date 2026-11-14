# Measuring a real GBA (AGB-001) for `assets/controller.svg`

The controller drawing comes from a dimension table in `tools/gen_controller_svg.py`. Every value is
either **from the owner's CAD** (tag `C`: the giant-GBA model in `../CAD`, built at 9.8067x, read
back at 1:1) or **provisional** (tag `P`: proportions of a reference drawing). Only *relative* sizes
matter, so the overall width is not needed. Rows marked *CAD* below are done; measuring the rest
turns them into first-hand values (tag `M`). This list is optional.

**How:** callipers, on the front face. **x** = from the console's **left** edge to the right,
**y** = from the **top** edge (top of the case, *not* counting the L/R buttons) downwards. Centres
are the middle of a round feature. Write the measured value in the last column; I'll update the
table, regenerate the SVG and show you the before/after. Numbers refer to the guide image
(`build/measure_guide.png`, made by `python3 tools/measure_guide.py`).

Still open: rows 1–3, 15–17 (outline, top slot, shoulders). Any subset helps.

| # | What | How | Provisional (mm) | Measured |
|---|---|---|---|---|
| 1 | Overall width | widest point, left grip to right grip | 144.5 (published) | |
| 2 | Overall height | top edge to the lowest point of the bottom edge (middle) | 82.0 (published) | |
| 3 | Bottom edge at the grips | top edge to the bottom edge, measured 15 mm in from the left end | 72.5 | |
| 4 | Screen lens, size | width × height of the dark lens window (plastic, not the LCD) | 76.1 × 66.3 | CAD: 75.63 × 64.10 |
| 5 | Screen lens, position | lens left edge (x) and top edge (y) | x 34.2, y 8.6 | CAD: x 34.44, y 8.98 |
| 6 | D-pad, size | tip-to-tip width; width of one arm | 16.9; 5.6 | CAD: 17.55; 5.75 |
| 7 | D-pad, centre | centre of the cross | x 20.2, y 34.7 | CAD: x 19.71, y 34.08 |
| 8 | A button | diameter; centre | ⌀ 8.6; x 131.4, y 31.5 | CAD: ⌀ 9.0; x 131.33, y 31.85 |
| 9 | B button | diameter; centre | ⌀ 8.6; x 119.3, y 35.9 | CAD: ⌀ 9.0; x 118.33, y 36.35 |
| 10 | START button (upper) | diameter of the round button; centre | ⌀ 4.0; x 30.1, y 54.6 | CAD: ⌀ 4.0; x 29.32, y 53.97 |
| 11 | SELECT button (lower) | diameter; centre | ⌀ 4.0; x 30.1, y 62.9 | CAD: ⌀ 4.0; x 29.32, y 61.97 |
| 12 | START/SELECT plate | length from the plate's left end to the button centre; plate thickness; slope (rough angle) | 13.0; 3.2; 14° | CAD: the recess is a 16.94 × 5.98 pill at 13.9°, centres x 24.0, y 52.72 / 60.72 |
| 13 | Power LED | centre; diameter | x 118.5, y 14.4; ⌀ 3.0 | CAD: x 118.4, y 14.55; ⌀ 2.37 |
| 14 | Speaker slots | number of slots; slot length; distance between slots; first slot's left end (x, y) | 6; 14.8; 3.0; x 115.2, y 51.0 | CAD: 5 slots, 16.72 × 1.57, pitch 3.0, 13.95°, centres x 121.59, y 50.79 down |
| 15 | Top slot (above the screen) | centre; length × height | x 72.3, y 4.4; 16.5 × 3.8 | |
| 16 | Top edge slope | top edge height (y) measured 15 mm and 30 mm in from the left end | 9.5; 5.0 | |
| 17 | Shoulder buttons | where the L button starts and ends along the top edge (x) | x 4.5 to 33.0 | |
| 18 | LCD (visible picture) | width × height (published 61.2 × 40.8) and its centre | 61.2 × 40.8; x 73.3, y 37.3 | CAD: 61.0 × 39.5; x 72.25, y 37.3 |

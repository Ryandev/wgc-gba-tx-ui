// The Settings zoom (SPEC.md §S, mockups/33_*): BG2 affine parameters for each step of zooming from
// the whole controller into its drawn LCD until the LCD fills the screen. Pure, host-tested.
// Fixed point 8.8 throughout, as the GBA's affine registers.
#pragma once
#include <stdint.h>

#define ZOOM_FRAMES 12  // about 200 ms
#define ZOOM_MAX    666 // 2.6x in 8.8: the 98 x 64 px LCD becomes 255 x 166, covering 240 x 160

typedef struct {
    int pa;       // BG2PA = BG2PD: picture pixels per screen pixel, 8.8 (256 = 1:1)
    int32_t x, y; // BG2X, BG2Y: picture point (8.8) shown at the screen's top-left pixel
} zoom_params;

// Ease-out progress for step k (0..ZOOM_FRAMES) in 1/256: 1 - (1 - k/N)^2, from a table.
unsigned zoom_ease(int k);

// Step k (0 = normal view, ZOOM_FRAMES = LCD fills the screen). from_x/y: the picture point (8.8) at
// the screen centre in the normal view; to_x/y: the LCD's centre (8.8). The focus point moves with
// the same easing as the scale, and is always shown at the screen centre (120, 80).
zoom_params zoom_at(int k, int32_t from_x, int32_t from_y, int32_t to_x, int32_t to_y);

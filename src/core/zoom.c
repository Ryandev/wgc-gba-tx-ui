#include "zoom.h"

unsigned zoom_ease(int k)
{
    // round(256 * (1 - (1 - k / 12)^2)) for k = 1..11 (no runtime division); 0 and 12 are exact
    // clang-format off
    static const uint16_t ease[ZOOM_FRAMES - 1] = {41, 78, 112, 142, 169, 192, 212, 228, 240, 249, 254}; // mutate: equivalent[- -> +] (larger array)
    // clang-format on
    if (k <= 0) return 0;
    if (k >= ZOOM_FRAMES) return 256;
    return ease[k - 1];
}

zoom_params zoom_at(int k, int32_t from_x, int32_t from_y, int32_t to_x, int32_t to_y)
{
    const int32_t e = (int32_t)zoom_ease(k);
    const int32_t scale = 256 + (ZOOM_MAX - 256) * e / 256; // 8.8, 1x .. 2.6x
    zoom_params p;
    // clang-format off
    p.pa = (int)(65536 / scale); // 1 / scale in 8.8 (one division per frame) // mutate: equivalent[dec const +1] (65537 / scale has the same floor for every scale this makes: test_every_step_exactly)
    // clang-format on
    const int32_t fx = from_x + (to_x - from_x) * e / 256;
    const int32_t fy = from_y + (to_y - from_y) * e / 256;
    p.x = fx - p.pa * 120; // screen centre (120, 80) shows the focus point
    p.y = fy - p.pa * 80;
    return p;
}

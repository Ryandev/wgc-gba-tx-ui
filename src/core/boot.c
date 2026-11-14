#include "boot.h"
#include "keyword.h"

boot_plan boot_decide(uint16_t pressed)
{
    boot_plan p; // every field is assigned below
    if ((pressed & (K_SELECT | K_DOWN)) == (K_SELECT | K_DOWN)) {
        p.screen = SCREEN_BLACK;
    } else if (pressed & K_SELECT) {
        p.screen = SCREEN_OFF;
    } else {
        p.screen = SCREEN_NORMAL;
    }
    // Settings needs a visible screen; with the screen off/black, Start is just a held key.
    p.settings = p.screen == SCREEN_NORMAL && (pressed & K_START);
    p.gb_mode = (pressed & K_A) != 0;
    return p;
}

#include "settings.h"
#include "keyword.h"

#define COMBO (K_L | K_R | K_SELECT)

void combo_init(combo_state *c)
{
    c->held = 0;
    c->armed = true;
}

bool combo_step(combo_state *c, uint16_t keys)
{
    if ((keys & COMBO) != COMBO) {
        c->held = 0;
        c->armed = true;
        return false;
    }
    if (!c->armed) return false;
    if (++c->held < COMBO_FRAMES) return false;
    c->armed = false;
    return true;
}

void settings_init(settings_state *s, bool boot, uint8_t channel, uint8_t address, uint8_t packets,
                   uint16_t keys)
{
    s->page = SET_HOME;
    s->row = HOME_PLAY;
    s->boot = boot;
    pairing_init(&s->pair, channel, address, packets);
    s->prev = keys;
}

static settings_action home(settings_state *s, uint16_t hit)
{
    if (hit & K_START) return SET_PLAY;
    if ((hit & K_B) && !s->boot) return SET_PLAY; // in-session: back = resume play
    if ((hit & K_UP) && s->row > 0) {
        s->row--;
        return SET_REDRAW;
    }
    if ((hit & K_DOWN) && s->row < HOME_ROWS - 1) {
        s->row++;
        return SET_REDRAW;
    }
    if (hit & K_A) {
        if (s->row == HOME_PLAY) return SET_PLAY;
        s->page = s->row == HOME_PAIRING ? SET_PAIRING : SET_ABOUT;
        return SET_REDRAW;
    }
    return SET_NONE;
}

settings_action settings_step(settings_state *s, uint16_t keys)
{
    const uint16_t hit = (uint16_t)(keys & ~s->prev);
    s->prev = keys;
    switch (s->page) {
    case SET_HOME:
        return home(s, hit);
    case SET_PAIRING:
        if (hit & K_B) {
            s->page = SET_HOME;
            return SET_REDRAW;
        }
        if (!s->boot) return SET_NONE; // in-session: read-only
        {
            const pairing_state before = s->pair;
            if (pairing_step(&s->pair, hit, keys) == PAIR_SEND) {
                s->page = SET_SENT;
                return SET_SEND;
            }
            const bool changed = before.row != s->pair.row || before.val[0] != s->pair.val[0] ||
                                 before.val[1] != s->pair.val[1] || before.val[2] != s->pair.val[2];
            return changed ? SET_REDRAW : SET_NONE;
        }
    case SET_ABOUT:
        if (hit & K_B) {
            s->page = SET_HOME;
            return SET_REDRAW;
        }
        return SET_NONE;
    default: // SET_SENT: stays until power-off
        return SET_NONE;
    }
}

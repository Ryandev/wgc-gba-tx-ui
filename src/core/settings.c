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

// Value lists (SPEC §K defaults: dim 30 s, sleep 2 min)
static const char *const dim_text[] = {"OFF", "15S", "30S", "1MIN", "2MIN"};
static const uint16_t dim_secs[] = {0, 15, 30, 60, 120};
static const char *const sleep_text[] = {"OFF", "1MIN", "2MIN", "5MIN", "10MIN"};
static const uint16_t sleep_secs[] = {0, 60, 120, 300, 600};
static const char *const screen_text[] = {"ON", "OFF", "BLACK"};
static const char *const notes_text[] = {"OFF", "ON"};

void settings_opts_default(settings_opts *o)
{
    o->val[OPT_DIM] = 2;   // 30 s
    o->val[OPT_SLEEP] = 2; // 2 min
    o->val[OPT_SCREEN] = SCREEN_ON;
    o->val[OPT_NOTES] = 1; // on
    o->gb_mode = false;
}

int settings_opt_count(int row)
{
    switch (row) { // no table to read past (mutation testing: a `<=` read one entry beyond it unseen)
    case OPT_DIM:
    case OPT_SLEEP:
        return 5;
    case OPT_SCREEN:
        return 3;
    case OPT_NOTES:
        return 2;
    default:
        return 0;
    }
}

const char *settings_opt_text(int row, uint8_t value)
{
    static const char *const *const text[DISPLAY_ROWS] = {dim_text, sleep_text, screen_text, notes_text};
    if (value >= settings_opt_count(row)) return "";
    return text[row][value];
}

uint32_t settings_dim_frames(const settings_opts *o) { return (uint32_t)dim_secs[o->val[OPT_DIM]] * 60u; }

uint32_t settings_sleep_frames(const settings_opts *o)
{
    return (uint32_t)sleep_secs[o->val[OPT_SLEEP]] * 60u;
}

void settings_init(settings_state *s, bool boot, uint8_t channel, uint8_t address, uint8_t packets,
                   const settings_opts *opts, uint16_t keys)
{
    s->page = SET_HOME;
    s->row = HOME_PLAY;
    s->sub = 0;
    s->boot = boot;
    pairing_init(&s->pair, channel, address, packets);
    s->opts = *opts;
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
        static const settings_page open[HOME_ROWS] = {SET_HOME, SET_PAIRING, SET_DISPLAY, SET_INPUT,
                                                      SET_ABOUT};
        if (s->row == HOME_PLAY) return SET_PLAY;
        s->page = open[s->row];
        s->sub = 0;
        return SET_REDRAW;
    }
    return SET_NONE;
}

static settings_action display(settings_state *s, uint16_t hit)
{
    if (hit & K_B) {
        s->page = SET_HOME;
        return SET_REDRAW;
    }
    if ((hit & K_UP) && s->sub > 0) {
        s->sub--;
        return SET_REDRAW;
    }
    if ((hit & K_DOWN) && s->sub < DISPLAY_ROWS - 1) {
        s->sub++;
        return SET_REDRAW;
    }
    uint8_t *v = &s->opts.val[s->sub];
    if ((hit & K_LEFT) && *v > 0) {
        (*v)--;
        return SET_REDRAW;
    }
    if ((hit & K_RIGHT) && *v < settings_opt_count(s->sub) - 1) {
        (*v)++;
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
    case SET_DISPLAY:
        return display(s, hit);
    case SET_INPUT:
        if (hit & K_B) {
            s->page = SET_HOME;
            return SET_REDRAW;
        }
        if (s->boot && (hit & (K_LEFT | K_RIGHT))) { // in-session: read-only (needs a restart)
            s->opts.gb_mode = !s->opts.gb_mode;
            return SET_REDRAW;
        }
        return SET_NONE;
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

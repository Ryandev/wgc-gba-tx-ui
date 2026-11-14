#include "text.h"

int fmt_u8(char *out, uint8_t v)
{
    int n = 0;
    if (v >= 100) out[n++] = (char)('0' + v / 100);
    if (v >= 10) out[n++] = (char)('0' + (v / 10) % 10);
    out[n++] = (char)('0' + v % 10);
    out[n] = 0;
    return n;
}

void fmt_hex3(char *out, uint16_t v)
{
    static const char h[] = "0123456789ABCDEF";
    out[0] = h[(v >> 8) & 15u];
    out[1] = h[(v >> 4) & 15u];
    out[2] = h[v & 15u];
    out[3] = 0;
}

void fmt_menu_row(char *out, size_t cap, const char *label, uint8_t value, int selected)
{
    if (cap == 0) return;
    char num[4]; // mutate: equivalent[dec const +1] (larger buffer changes nothing)
    size_t n = 0;
    const char *parts[3] = {selected ? ">" : " ", label, ": "}; // mutate: equivalent[dec const +1]
    for (int i = 0; i < 3; i++)
        for (const char *p = parts[i]; *p && n + 1 < cap; p++) out[n++] = *p;
    fmt_u8(num, value);
    for (const char *p = num; *p && n + 1 < cap; p++) out[n++] = *p;
    out[n] = 0;
}

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

int fmt_pad2(char *out, uint8_t v)
{
    if (v >= 10) return fmt_u8(out, v);
    out[0] = '0';
    out[1] = (char)('0' + v);
    out[2] = 0;
    return 2;
}

void fmt_append(char *out, int n, char c)
{
    out[n] = c;
    out[n + 1] = 0;
}

void fmt_ghz(char out[10], uint8_t channel)
{
    // 2400 + channel MHz as "2.4CC GHZ" (100..125 -> "2.5CC GHZ"; the nRF24 goes up to 2.525 GHz)
    static const char model[] = "2.4CC GHZ";
    for (int i = 0; i < 10; i++) out[i] = model[i];
    out[2] = (char)('4' + channel / 100);
    out[3] = (char)('0' + channel / 10 % 10);
    out[4] = (char)('0' + channel % 10);
}

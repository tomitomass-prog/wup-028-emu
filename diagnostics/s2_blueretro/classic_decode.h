#ifndef CLASSIC_DECODE_H
#define CLASSIC_DECODE_H

#include <stdbool.h>
#include <stdint.h>

/* Nintendo Wii Classic Controller, unencrypted data format 0x01.
 * WUP-028 port payload: status, buttons-low, buttons-high,
 * stick-x, stick-y, c-x, c-y, analog-L, analog-R.
 * Buttons are active low in Classic reports, active high in WUP-028.
 */
static inline uint8_t classic_scale(unsigned value, unsigned max) {
    if (value > max) value = max;
    return (uint8_t)((value * 255u + max / 2u) / max);
}

static inline void classic_neutral(uint8_t out[9]) {
    for (unsigned i = 0; i < 9; ++i) out[i] = 0;
    out[0] = 0x10; /* wired GC controller in USB port 1 */
    out[3] = out[4] = out[5] = out[6] = 128;
}

static inline void classic_decode(const uint8_t d[6], uint8_t out[9]) {
    classic_neutral(out);
    const uint8_t p4 = (uint8_t)~d[4];
    const uint8_t p5 = (uint8_t)~d[5];
    uint8_t lo = 0, hi = 0;

    if (p5 & (1u << 4)) lo |= 0x01; /* Classic A -> GC A */
    if (p5 & (1u << 6)) lo |= 0x02; /* B -> B */
    if (p5 & (1u << 3)) lo |= 0x04; /* X -> X */
    if (p5 & (1u << 5)) lo |= 0x08; /* Y -> Y */
    if (p5 & (1u << 1)) lo |= 0x10; /* dpad left */
    if (p4 & (1u << 7)) lo |= 0x20; /* dpad right */
    if (p4 & (1u << 6)) lo |= 0x40; /* dpad down */
    if (p5 & (1u << 0)) lo |= 0x80; /* dpad up */

    if (p4 & (1u << 2)) hi |= 0x01; /* plus -> GC START */
    if (p5 & ((1u << 2) | (1u << 7))) hi |= 0x02; /* ZR/ZL -> Z */
    if (p4 & (1u << 1)) hi |= 0x04; /* R click -> R */
    if (p4 & (1u << 5)) hi |= 0x08; /* L click -> L */

    const unsigned lx = d[0] & 0x3fu;
    const unsigned ly = d[1] & 0x3fu;
    const unsigned rx = (((unsigned)d[0] >> 6) & 3u) << 3
                      | (((unsigned)d[1] >> 6) & 3u) << 1
                      | (((unsigned)d[2] >> 7) & 1u);
    const unsigned ry = d[2] & 0x1fu;
    const unsigned lt = ((((unsigned)d[2] >> 5) & 3u) << 3)
                      | (((unsigned)d[3] >> 5) & 7u);
    const unsigned rt = d[3] & 0x1fu;

    out[1] = lo;
    out[2] = hi;
    out[3] = classic_scale(lx, 63);
    out[4] = classic_scale(ly, 63);
    out[5] = classic_scale(rx, 31);
    out[6] = classic_scale(ry, 31);
    out[7] = classic_scale(lt, 31);
    out[8] = classic_scale(rt, 31);
}
#endif

/* Segment 42a0 – Bitflags in 16-Bit-Wörtern */
#include "game.h"

/* 42a0:0001 */
void bit_set(uint16_t *w, int16_t bit)
{
    *w |= (uint16_t)(1u << (bit & 0x1f));
}

/* 42a0:0016 */
void bit_clear(uint16_t *w, int16_t bit)
{
    *w &= (uint16_t)~(1u << (bit & 0x1f));
}

/* 42a0:002d */
int16_t bit_test(uint16_t w, int16_t bit)
{
    return (w & (1u << (bit & 0x1f))) != 0;
}

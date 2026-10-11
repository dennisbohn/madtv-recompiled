/* Segment 3b06 – kleine Hilfsfunktionen. */
#include "game.h"

/* 3b06:0000 */
int16_t min_i16(int16_t a, int16_t b)
{
    return b < a ? b : a;
}

/* 3b06:0015 */
int16_t max_i16(int16_t a, int16_t b)
{
    return b < a ? a : b;
}

/* 3b06:0027 */
int32_t max_i32(int32_t a, int32_t b)
{
    return b < a ? a : b;
}

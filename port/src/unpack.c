#include "unpack.h"

#include <stdlib.h>
#include <string.h>

enum { LZW_CLEAR = 0x100, LZW_GROW = 0x101, LZW_FIRST = 0x102, LZW_MAX = 1 << 16 };

typedef struct {
    const uint8_t *src;
    size_t size, pos;
    int bitpos, width;
    long remaining; /* Byte-Zähler wie im Original (a10a) */
} LzwIn;

static unsigned lzw_read(LzwIn *in)
{
    unsigned code = 0;
    int need = in->width;
    while (need > 0) {
        int take = 8 - in->bitpos < need ? 8 - in->bitpos : need;
        unsigned byte = in->pos < in->size ? in->src[in->pos] : 0;
        code = (code << take) | ((byte >> (8 - in->bitpos - take)) & ((1u << take) - 1));
        need -= take;
        in->bitpos += take;
        if (in->bitpos == 8) {
            in->bitpos = 0;
            in->pos++;
            in->remaining--;
        }
    }
    return code;
}

typedef struct {
    uint8_t *buf;
    size_t len, cap;
    uint16_t *prefix;
    uint8_t *first, *last;
    uint8_t *stack;
} LzwOut;

static int lzw_emit(LzwOut *o, unsigned code)
{
    size_t n = 0;
    for (;;) {
        if (n >= LZW_MAX)
            return -1;
        o->stack[n++] = o->last[code];
        if (o->prefix[code] == 0xFFFF)
            break;
        code = o->prefix[code];
    }
    if (o->len + n > o->cap) {
        size_t cap = o->cap * 2 + n;
        uint8_t *nb = realloc(o->buf, cap);
        if (!nb)
            return -1;
        o->buf = nb;
        o->cap = cap;
    }
    while (n > 0)
        o->buf[o->len++] = o->stack[--n];
    return 0;
}

uint8_t *lzw_unpack(const uint8_t *src, size_t src_size, size_t *out_size)
{
    if (src_size < 8)
        return NULL;
    LzwIn in = {src, src_size, 8, 0, 9, src[2] | (src[3] << 8)};
    LzwOut o = {0};
    o.cap = (size_t)(src[0] | (src[1] << 8)) + 16;
    o.buf = malloc(o.cap);
    o.prefix = malloc(LZW_MAX * sizeof *o.prefix);
    o.first = malloc(LZW_MAX);
    o.last = malloc(LZW_MAX);
    o.stack = malloc(LZW_MAX);
    int ok = o.buf && o.prefix && o.first && o.last && o.stack;

    if (ok) {
        for (unsigned i = 0; i < 256; i++) {
            o.prefix[i] = 0xFFFF;
            o.first[i] = o.last[i] = (uint8_t)i;
        }
        unsigned next = LZW_FIRST;
        unsigned cur = lzw_read(&in), prev = cur;
        ok = cur < 256 && lzw_emit(&o, cur) == 0;
        while (ok) {
            cur = lzw_read(&in);
            if (in.remaining <= 0)
                break;
            if (cur == LZW_CLEAR) {
                in.width = 9;
                next = LZW_FIRST;
                cur = prev = lzw_read(&in);
                ok = cur < 256 && lzw_emit(&o, cur) == 0;
                continue;
            }
            if (cur == LZW_GROW) {
                in.width++;
                ok = in.width <= 16;
                continue;
            }
            if (next >= LZW_MAX || cur > next || (cur >= 256 && cur < LZW_FIRST)) {
                ok = 0;
                break;
            }
            o.prefix[next] = (uint16_t)prev;
            o.first[next] = o.first[prev];
            o.last[next] = o.first[cur]; /* bei cur == next ist first[next] schon gesetzt */
            next++;
            prev = cur;
            ok = lzw_emit(&o, cur) == 0;
        }
    }

    free(o.prefix);
    free(o.first);
    free(o.last);
    free(o.stack);
    if (!ok) {
        free(o.buf);
        return NULL;
    }
    if (out_size)
        *out_size = o.len;
    return o.buf;
}

int rle_delta_frame(const uint8_t **stream, const uint8_t *end, uint8_t *dst, size_t dst_size)
{
    const uint8_t *s = *stream;
    size_t di = 0;
    while (s < end) {
        uint8_t c = *s++;
        if (c == 0x62) {
            *stream = s;
            return 0;
        }
        if (c == 0x63) {
            size_t n = 0;
            uint8_t b;
            do {
                if (s >= end)
                    return -1;
                b = *s++;
                n += b;
            } while (b == 0xFF);
            if (s >= end)
                return -1;
            uint8_t v = *s++;
            if (di + n > dst_size)
                return -1;
            if (v != 0x64)
                memset(dst + di, v, n);
            di += n;
        } else if (c == 0x64) {
            di++;
        } else {
            if (di >= dst_size)
                return -1;
            dst[di++] = c;
        }
    }
    return -1;
}

#include "hes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_dir[512] = "Gamefiles";

void hes_set_dir(const char *dir)
{
    snprintf(g_dir, sizeof g_dir, "%s", dir);
}

static void hes_path(int no, char *buf, size_t len)
{
    snprintf(buf, len, "%s/%03d.HES", g_dir, no);
}

int hes_exists(int no)
{
    char path[600];
    hes_path(no, path, sizeof path);
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;
    fclose(f);
    return 1;
}

uint8_t *hes_file_load(int no, size_t *size)
{
    char path[600];
    hes_path(no, path, sizeof path);
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(len > 0 ? (size_t)len : 1);
    if (buf && fread(buf, 1, (size_t)len, f) != (size_t)len) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    if (buf && size)
        *size = (size_t)len;
    return buf;
}

int hes_load_palette(int no, uint8_t rgb[768])
{
    size_t size;
    uint8_t *d = hes_file_load(no, &size);
    if (!d)
        return -1;
    int n = d[0] | (d[1] << 8);
    if (n > 256 || size < 2 + (size_t)n * 3) {
        free(d);
        return -1;
    }
    memset(rgb, 0, 768);
    for (int i = 0; i < n * 3; i++)
        rgb[i] = (uint8_t)((d[2 + i] & 63) * 255 / 63);
    free(d);
    return 0;
}

/* ---- Huffman-Sprite-Bänke ------------------------------------------------
 * Datei:  u8 N; N x {u8 w, u8 h}; u16be nodes; u32be out_len; Bitstrom.
 * Bits werden LSB-first gelesen. Es gibt einen Startbaum und 256 Kontextbäume
 * (Kontext = zuletzt ausgegebenes Byte). Im Baum-Bitstrom bedeutet 0 einen
 * inneren Knoten (links, dann rechts), 1 ein Blatt; das erste Blatt eines Baums
 * ist das Escape-Blatt, jedes weitere trägt ein 8-Bit-Literal.
 * Escape im Startbaum -> 8 Rohbits, Escape im Kontextbaum -> Startbaum. */

enum { ESC = 0x100, FIRST_NODE = 0x101 };

typedef struct {
    const uint8_t *p, *end;
    unsigned cur;
    int left;
    int overrun;
    uint16_t *lchild, *rchild; /* indiziert mit (node - FIRST_NODE) */
    int next_node, max_nodes;
    int esc_used;
} Huff;

static int huf_bit(Huff *h)
{
    if (h->left == 0) {
        if (h->p >= h->end) {
            h->overrun = 1;
            return 0;
        }
        h->cur = *h->p++;
        h->left = 7;
    } else {
        h->cur >>= 1;
        h->left--;
    }
    return h->cur & 1;
}

static int huf_byte(Huff *h)
{
    int v = 0;
    for (int i = 0; i < 8; i++)
        v = ((huf_bit(h) << 7) | (v >> 1)) & 0xFF;
    return v;
}

static int huf_tree(Huff *h, int depth)
{
    if (depth > 300 || h->overrun)
        return -1;
    if (huf_bit(h) == 0) {
        int node = h->next_node++;
        if (node - FIRST_NODE >= h->max_nodes)
            return -1;
        int l = huf_tree(h, depth + 1);
        int r = huf_tree(h, depth + 1);
        if (l < 0 || r < 0)
            return -1;
        h->lchild[node - FIRST_NODE] = (uint16_t)l;
        h->rchild[node - FIRST_NODE] = (uint16_t)r;
        return node;
    }
    if (!h->esc_used) {
        h->esc_used = 1;
        return ESC;
    }
    return huf_byte(h);
}

static int huf_walk(Huff *h, int node)
{
    while (node > ESC) {
        int i = node - FIRST_NODE;
        node = huf_bit(h) ? h->rchild[i] : h->lchild[i];
    }
    return node;
}

int hes_is_sprite_bank(const uint8_t *d, size_t size)
{
    if (size < 8)
        return 0;
    int n = d[0];
    size_t hdr = 1 + 2 * (size_t)n;
    if (n == 0 || size < hdr + 6)
        return 0;
    uint32_t total = 0;
    for (int i = 0; i < n; i++)
        total += (uint32_t)d[1 + 2 * i] * d[2 + 2 * i];
    uint32_t out_len = ((uint32_t)d[hdr + 2] << 24) | ((uint32_t)d[hdr + 3] << 16) |
                       ((uint32_t)d[hdr + 4] << 8) | d[hdr + 5];
    return total > 0 && total == out_len;
}

int hes_decode_sprites(const uint8_t *d, size_t size, HesSpriteSet *out)
{
    memset(out, 0, sizeof *out);
    if (!hes_is_sprite_bank(d, size))
        return -1;

    int n = d[0];
    size_t hdr = 1 + 2 * (size_t)n;
    int nodes = (d[hdr] << 8) | d[hdr + 1];
    uint32_t out_len = ((uint32_t)d[hdr + 2] << 24) | ((uint32_t)d[hdr + 3] << 16) |
                       ((uint32_t)d[hdr + 4] << 8) | d[hdr + 5];

    Huff h = {0};
    h.p = d + hdr + 6;
    h.end = d + size;
    h.max_nodes = nodes; /* Original: calloc(nodes, 2) je Kindtabelle */
    h.next_node = FIRST_NODE;
    h.lchild = calloc((size_t)nodes + 1, sizeof(uint16_t));
    h.rchild = calloc((size_t)nodes + 1, sizeof(uint16_t));
    uint8_t *buf = malloc(out_len);
    int ctx[256];
    int rc = -1;
    if (!h.lchild || !h.rchild || !buf)
        goto done;

    h.esc_used = 0;
    int start = huf_tree(&h, 0);
    if (start < 0)
        goto done;
    for (int i = 0; i < 256; i++) {
        h.esc_used = 0;
        if ((ctx[i] = huf_tree(&h, 0)) < 0)
            goto done;
    }

    int sym = huf_walk(&h, start);
    if (sym == ESC)
        sym = huf_byte(&h);
    buf[0] = (uint8_t)sym;
    for (uint32_t o = 1; o < out_len; o++) {
        sym = huf_walk(&h, ctx[sym]);
        if (sym == ESC) {
            sym = huf_walk(&h, start);
            if (sym == ESC)
                sym = huf_byte(&h);
        }
        buf[o] = (uint8_t)sym;
    }
    if (h.overrun)
        goto done;

    out->count = n;
    out->sprites = calloc((size_t)n, sizeof(HesSprite));
    if (!out->sprites)
        goto done;
    out->storage = buf;
    buf = NULL;
    uint8_t *px = out->storage;
    for (int i = 0; i < n; i++) {
        out->sprites[i].w = d[1 + 2 * i];
        out->sprites[i].h = d[2 + 2 * i];
        out->sprites[i].pixels = px;
        px += out->sprites[i].w * out->sprites[i].h;
    }
    rc = 0;

done:
    free(h.lchild);
    free(h.rchild);
    free(buf);
    if (rc != 0)
        hes_free_sprites(out);
    return rc;
}

void hes_free_sprites(HesSpriteSet *bank)
{
    free(bank->sprites);
    free(bank->storage);
    memset(bank, 0, sizeof *bank);
}

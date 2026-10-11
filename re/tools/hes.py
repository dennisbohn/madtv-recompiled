"""Prototyp: HES-Dateien von MadTV klassifizieren und Sprite-Bänke nach PNG entpacken.

Nachbau von hes_unpack_sprites (MTV.EXE 4444:04ac):
  Datei:  u8 N; N x {u8 w, u8 h}; u16be nodes; u32be out_len; Bitstrom (LSB-first)
  Bäume:  Startbaum + 256 Kontextbäume (Kontext = vorheriges Ausgabebyte).
          Bit 0 = innerer Knoten (links, rechts rekursiv), Bit 1 = Blatt.
          Das erste Blatt jedes Baums ist das Escape-Blatt (0x100), alle weiteren
          tragen ein 8-Bit-Literal.
  Escape: im Startbaum -> 8 Rohbits; im Kontextbaum -> Startbaum dekodieren.

Aufruf: python -I hes.py <Gamefiles-Ordner> <Ausgabeordner>
"""
import os
import struct
import sys
import zlib

ESC = 0x100


class Bits:
    def __init__(self, data, pos):
        self.d, self.p, self.cur, self.n = data, pos, 0, 0

    def bit(self):
        if self.n == 0:
            self.cur = self.d[self.p]
            self.p += 1
            self.n = 7
        else:
            self.cur >>= 1
            self.n -= 1
        return self.cur & 1

    def byte(self):
        v = 0
        for _ in range(8):
            v = ((self.bit() << 7) | (v >> 1)) & 0xFF
        return v


def unpack_sprites(data):
    n = data[0]
    dims = [(data[1 + 2 * i], data[2 + 2 * i]) for i in range(n)]
    pos = 1 + 2 * n
    nodes = struct.unpack_from(">H", data, pos)[0]
    out_len = struct.unpack_from(">I", data, pos + 2)[0]
    bits = Bits(data, pos + 6)

    left, right = {}, {}
    next_id = [0x101]

    def read_tree(state):
        if bits.bit() == 0:
            node = next_id[0]
            next_id[0] += 1
            left[node] = read_tree(state)
            right[node] = read_tree(state)
            return node
        if not state["esc"]:
            state["esc"] = True
            return ESC
        return bits.byte()

    start = read_tree({"esc": False})
    ctx = [read_tree({"esc": False}) for _ in range(256)]

    def walk(node):
        while node > ESC:
            node = right[node] if bits.bit() else left[node]
        return node

    out = bytearray()
    sym = walk(start)
    if sym == ESC:
        sym = bits.byte()
    out.append(sym)
    while len(out) < out_len:
        sym = walk(ctx[sym])
        if sym == ESC:
            sym = walk(start)
            if sym == ESC:
                sym = bits.byte()
        out.append(sym)

    sprites, off = [], 0
    for w, h in dims:
        sprites.append((w, h, bytes(out[off:off + w * h])))
        off += w * h
    return sprites, nodes


def is_sprite_bank(data):
    if len(data) < 8:
        return False
    n = data[0]
    if n == 0 or len(data) < 1 + 2 * n + 6:
        return False
    total = sum(data[1 + 2 * i] * data[2 + 2 * i] for i in range(n))
    out_len = struct.unpack_from(">I", data, 1 + 2 * n + 2)[0]
    return total == out_len and total > 0


def is_palette(data):
    return len(data) == 770 and data[:2] == b"\x00\x01" and max(data[2:]) < 64


def write_png(path, w, h, pixels, pal):
    raw = b"".join(b"\x00" + pixels[y * w:(y + 1) * w] for y in range(h))
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 3, 0, 0, 0))
    png += chunk(b"PLTE", bytes(pal)) + chunk(b"tRNS", b"\x00") + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def main(game_dir, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    pal_raw = open(os.path.join(game_dir, "001.HES"), "rb").read()[2:]
    pal = [min(255, v * 255 // 63) for v in pal_raw]

    report = []
    for name in sorted(os.listdir(game_dir)):
        if not name.upper().endswith(".HES"):
            continue
        data = open(os.path.join(game_dir, name), "rb").read()
        kind = "?"
        if data.startswith(b"PALLADIX"):
            kind = "code (PSM-Treiber)"
        elif is_palette(data):
            kind = "palette"
        elif is_sprite_bank(data):
            try:
                sprites, _ = unpack_sprites(data)
                kind = "sprites x%d" % len(sprites)
                # alle Sprites einer Bank als Kontaktbogen nebeneinander
                W = sum(w for w, _, _ in sprites) + 2 * len(sprites)
                H = max(h for _, h, _ in sprites)
                sheet = bytearray(W * H)
                x0 = 0
                for w, h, px in sprites:
                    for y in range(h):
                        sheet[y * W + x0:y * W + x0 + w] = px[y * w:(y + 1) * w]
                    x0 += w + 2
                write_png(os.path.join(out_dir, name[:-4] + ".png"), W, H, sheet, pal)
            except Exception as e:  # noqa: BLE001 - Prototyp: Fehler nur melden
                kind = "sprites FEHLER: %r" % e
        report.append("%s\t%6d\t%s" % (name, len(data), kind))
    with open(os.path.join(out_dir, "_index.tsv"), "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    print("\n".join(report))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])

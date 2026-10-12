"""Erzeugt res/madtv.ico: Pixel-Art-Fernseher (32x32-Raster), Größen 16, 32, 64, 256.

Aufruf (aus port/): python -I res/make_icon.py
"""
import os
import struct
import zlib

W = H = 32
T = None  # transparent
OUT = (60, 25, 15)        # Umriss
WOOD = (176, 82, 44)      # Gehäuse
WOOD_HI = (214, 120, 70)  # Lichtkante
WOOD_LO = (130, 55, 30)   # Schattenkante
GLASS_D = (20, 40, 90)    # Bildschirm dunkel
GLASS_M = (40, 90, 190)
GLASS_L = (110, 190, 255)
SHINE = (220, 245, 255)
GRILLE = (90, 40, 20)
KNOB = (240, 205, 80)
KNOB_D = (150, 110, 30)
ANT = (70, 70, 70)

img = [[T] * W for _ in range(H)]


def px(x, y, c):
    if 0 <= x < W and 0 <= y < H:
        img[y][x] = c


def rect(x0, y0, x1, y1, c):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            px(x, y, c)


def line(x0, y0, x1, y1, c):
    n = max(abs(x1 - x0), abs(y1 - y0))
    for i in range(n + 1):
        px(round(x0 + (x1 - x0) * i / n), round(y0 + (y1 - y0) * i / n), c)


# Antenne
line(16, 8, 9, 1, ANT)
line(16, 8, 23, 1, ANT)
px(9, 1, KNOB)
px(23, 1, KNOB)
# Gehäuse mit Umriss, Licht- und Schattenkante
rect(1, 8, 30, 28, OUT)
rect(2, 9, 29, 27, WOOD)
rect(2, 9, 29, 9, WOOD_HI)
rect(2, 9, 2, 27, WOOD_HI)
rect(2, 27, 29, 27, WOOD_LO)
rect(29, 9, 29, 27, WOOD_LO)
# Bildschirm (abgerundete Ecken), Verlauf und Glanz
rect(4, 11, 21, 25, OUT)
for y in range(12, 25):
    for x in range(5, 21):
        t = (x - 5 + y - 12) / 28.0
        px(x, y, GLASS_L if t < 0.3 else GLASS_M if t < 0.7 else GLASS_D)
for x, y in ((5, 12), (20, 12), (5, 24), (20, 24)):
    px(x, y, OUT)
for i in range(4):
    px(7 + i, 14, SHINE)
px(7, 15, SHINE)
px(7, 16, SHINE)
# Lautsprechergitter
for y in range(12, 20, 2):
    rect(23, y, 27, y, GRILLE)
# Knöpfe
for kx in (23, 26):
    rect(kx, 22, kx + 1, 23, KNOB)
    px(kx + 1, 23, KNOB_D)
# Füße
rect(4, 29, 6, 30, OUT)
rect(25, 29, 27, 30, OUT)


def png(size):
    f = size // W if size >= W else None
    rows = []
    for y in range(size):
        row = bytearray([0])
        for x in range(size):
            if f:
                c = img[y // f][x // f]
                row += bytes(c) + b'\xff' if c else b'\x00\x00\x00\x00'
            else:  # verkleinern: 2x2-Mittel (16x16)
                s = W // size
                cs = [img[y * s + j][x * s + i] for j in range(s) for i in range(s)]
                opaque = [c for c in cs if c]
                if len(opaque) * 2 < len(cs):
                    row += b'\x00\x00\x00\x00'
                else:
                    row += bytes(sum(c[k] for c in opaque) // len(opaque) for k in range(3)) + b'\xff'
        rows.append(bytes(row))

    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(b''.join(rows), 9)) + chunk(b'IEND', b''))


sizes = [256, 64, 32, 16]
pngs = [png(s) for s in sizes]
ico = struct.pack('<HHH', 0, 1, len(sizes))
off = 6 + 16 * len(sizes)
for s, p in zip(sizes, pngs):
    d = 0 if s >= 256 else s
    ico += struct.pack('<BBBBHHII', d, d, 0, 0, 1, 32, len(p), off)
    off += len(p)
ico += b''.join(pngs)
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'madtv.ico')
open(out, 'wb').write(ico)
print(out, len(ico))

"""8-Bit-BMP (Screenshot des Ports) in PNG umwandeln, optional vergrößert.

Aufruf: python -I bmp2png.py ein.bmp aus.png [faktor]
"""
import struct
import sys
import zlib


def main(src, dst, scale=1):
    d = open(src, "rb").read()
    off = struct.unpack_from("<I", d, 10)[0]
    w, h = struct.unpack_from("<ii", d, 18)
    pal = d[54:54 + 1024]
    stride = (w + 3) & ~3
    rows = []
    for y in range(abs(h)):
        sy = (abs(h) - 1 - y) if h > 0 else y
        px = d[off + sy * stride: off + sy * stride + w]
        row = b"".join(bytes((pal[i * 4 + 2], pal[i * 4 + 1], pal[i * 4])) * scale for i in px)
        for _ in range(scale):
            rows.append(b"\x00" + row)

    def chunk(t, x):
        return struct.pack(">I", len(x)) + t + x + struct.pack(">I", zlib.crc32(t + x) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w * scale, abs(h) * scale, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(b"".join(rows))) + chunk(b"IEND", b"")
    open(dst, "wb").write(png)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 1)

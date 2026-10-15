"""Prototyp: LZW-Entpacker aus MTV.EXE 438d:0002 nachgebaut.

Quelle: +2 u16le = Anzahl komprimierter Bytes, Daten ab +8.
Codes MSB-first, Startbreite 9 Bit; 0x100 = Reset (Breite 9, nächster Code 0x102),
0x101 = Codebreite + 1. Das Original verarbeitet einen Code nur, solange der
Byte-Zähler nach dem Lesen noch nicht 0 ist.

Aufruf: python -I lzw.py <Ordner mit NNN.HES der Beta, re/v1>   -> listet Dateien, die sich sauber entpacken lassen
"""
import os
import struct
import sys


def lzw_decode(src):
    remaining = struct.unpack_from("<H", src, 2)[0]
    p = 8
    bitpos = 0
    width = 9

    def read_code():
        nonlocal p, bitpos, remaining
        need = width
        code = 0
        while need > 0:
            take = min(8 - bitpos, need)
            byte = src[p] if p < len(src) else 0
            bits = (byte >> (8 - bitpos - take)) & ((1 << take) - 1)
            code = (code << take) | bits
            need -= take
            bitpos += take
            if bitpos == 8:
                bitpos = 0
                p += 1
                remaining -= 1
        return code

    # Hinweis: Das Original zählt 'remaining' beim Lesen des ersten (Teil-)Bytes herunter,
    # nicht beim Verlassen; zur Stopp-Bedingung reicht der Vergleich auf <= 0 nach dem Lesen.
    prefix = [0xFFFF] * 4096
    first = list(range(256)) + [0] * (4096 - 256)
    last = list(range(256)) + [0] * (4096 - 256)
    out = bytearray()

    def emit(code):
        stack = []
        while True:
            stack.append(last[code])
            nxt = prefix[code]
            if nxt == 0xFFFF:
                break
            code = nxt
        out.extend(reversed(stack))

    nxt = 0x102
    cur = read_code()
    prev = cur
    emit(cur)
    while True:
        cur = read_code()
        if remaining <= 0:
            break
        if cur == 0x100:
            width = 9
            nxt = 0x102
            cur = read_code()
            prev = cur
            emit(cur)
            continue
        if cur == 0x101:
            width += 1
            continue
        if nxt >= 4096 or cur > nxt:
            raise ValueError("ungültiger Code %x (next %x)" % (cur, nxt))
        prefix[nxt] = prev
        first[nxt] = first[prev]
        last[nxt] = first[cur]
        nxt += 1
        prev = cur
        emit(cur)
    return bytes(out)


if __name__ == "__main__":
    d = sys.argv[1]
    for name in sorted(os.listdir(d)):
        data = open(os.path.join(d, name), "rb").read()
        if len(data) < 9:
            continue
        clen = struct.unpack_from("<H", data, 2)[0]
        if abs(clen - (len(data) - 8)) > 2:
            continue
        try:
            out = lzw_decode(data)
            u32 = struct.unpack_from("<I", data, 4)[0]
            print("%s\tin=%d\tout=%d\thdr0=%s\thdr4=%d" % (name, len(data), len(out), data[:2].hex(), u32))
        except Exception as e:  # noqa: BLE001
            print("%s\tFEHLER %s" % (name, e))

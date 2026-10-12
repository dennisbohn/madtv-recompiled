"""LZEXE-0.91-Entpacker (Format wie unlzexe.c): liefert das entpackte Programmabbild.

Aufruf: python -I unlzexe.py PACKED.EXE [OUT.EXE]
Ohne OUT nur Kopfdaten ausgeben. OUT erhält eine MZ-Datei mit dem entpackten Abbild
(Relokationen werden mit übernommen).
"""
import struct
import sys


def unlzexe(data):
    hdr = struct.unpack_from('<14H', data, 0)
    head_para, cs = hdr[4], hdr[11]
    lz = (head_para + cs) * 16
    if data[0x1c:0x20] != b'LZ91':
        raise ValueError('kein LZEXE 0.91')
    ip0, cs0, sp0, ss0, comp_para = struct.unpack_from('<5H', data, lz)
    pos = (head_para + cs - comp_para) * 16

    out = bytearray()
    bitbuf = struct.unpack_from('<H', data, pos)[0]
    pos += 2
    count = 16

    def getbit():
        nonlocal bitbuf, count, pos
        b = bitbuf & 1
        count -= 1
        if count == 0:
            bitbuf = struct.unpack_from('<H', data, pos)[0]
            pos += 2
            count = 16
        else:
            bitbuf >>= 1
        return b

    while True:
        if getbit():
            out.append(data[pos])
            pos += 1
            continue
        if not getbit():
            ln = (getbit() << 1) | getbit()
            ln += 2
            span = data[pos] | 0xff00
            pos += 1
        else:
            lo, hi = data[pos], data[pos + 1]
            pos += 2
            span = lo | ((hi & ~7 & 0xff) << 5) | 0xe000
            ln = (hi & 7) + 2
            if ln == 2:
                ln = data[pos]
                pos += 1
                if ln == 0:
                    break
                if ln == 1:
                    continue
                ln += 1
        span -= 0x10000  # negativer Abstand
        for _ in range(ln):
            out.append(out[len(out) + span])

    # Relokationen (Version 0.91: ab CS:0x158)
    rp = lz + 0x158
    rel = []
    roff = rseg = 0
    while True:
        span = data[rp]
        rp += 1
        if span == 0:
            span = struct.unpack_from('<H', data, rp)[0]
            rp += 2
            if span == 0:
                rseg += 0x0fff
                continue
            if span == 1:
                break
        roff += span
        rseg += (roff & ~0x0f) >> 4
        roff &= 0x0f
        rel.append((roff, rseg))
    return bytes(out), (ip0, cs0, sp0, ss0), rel


def write_mz(path, image, regs, rel):
    ip0, cs0, sp0, ss0 = regs
    nrel = len(rel)
    head_para = (0x1c + nrel * 4 + 15) // 16
    total = head_para * 16 + len(image)
    hdr = struct.pack('<14H', 0x5a4d, total % 512, (total + 511) // 512, nrel, head_para, 0, 0xffff,
                      ss0, sp0, 0, ip0, cs0, 0x1c, 0)
    buf = bytearray(hdr)
    for o, s in rel:
        buf += struct.pack('<HH', o, s)
    buf += b'\0' * (head_para * 16 - len(buf))
    buf += image
    open(path, 'wb').write(buf)


if __name__ == '__main__':
    d = open(sys.argv[1], 'rb').read()
    img, regs, rel = unlzexe(d)
    print('Abbild %d Byte, CS:IP %04x:%04x SS:SP %04x:%04x, %d Relokationen' % (len(img), regs[1], regs[0], regs[3], regs[2], len(rel)))
    if len(sys.argv) > 2:
        write_mz(sys.argv[2], img, regs, rel)

"""Datensegment aus MTV.EXE (Release-Fassung) im Layout des Ports, für die Prüfung von
src/game/ds_init.c (tools/dscheck.c, "make dscheck").

Aufruf (aus der Projektwurzel): python -I re/tools/dgroup_extract.py gamedata/MTV.EXE OUT.bin
"""
import hashlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from unlzexe import unlzexe  # noqa: E402

# Der Port ist aus dem Code der ersten Fassung von MTV.EXE übersetzt und spricht das Datensegment
# über deren Adressen an (DS_*-Makros). Die Release-Fassung ist neu übersetzt; ihr Datensegment
# enthält dieselben Spieldaten, aber ab DS:057d um einige Byte verschoben. Abschnitte (Adresse im
# Port, Adresse in der Release-Fassung, Länge), ermittelt durch Abgleich beider Segmente:
SECTIONS = [(0x0000, 0x0000, 0x057d), (0x057d, 0x057f, 0x1235),
            (0x17c3, 0x17b7, 0x005d), (0x182c, 0x1820, 0x0115), (0x1941, 0x1961, 0x0009),
            (0x194a, 0x198a, 0x8ef7)]
DGROUP_LEN = 0xa841
# Dazwischen liegen nur Werte der Laufzeitbibliothek, die der Port nicht nutzt (der Dateiname
# "000.HES" der ersten Fassung, Far-Zeiger der Exit-Hooks); sie werden wie in der ersten Fassung
# gesetzt. Ab etwa DS:1950 ist das Segment genullt (Variablen ohne Startwert); die Abschnitte dort
# ordnen nur Nullen zu, die Variablen selbst liegen in der Release-Fassung um 0x20 verschoben
# (re/tools/v1v2_map.py).
FILLS = [(0x17b2, bytes.fromhex("48455300") + b"000.HES" + bytes(6)),
         (0x1820, bytes.fromhex('0f0019350f0019350f001935'))]
DGROUP_SHA256 = '157de2f22618bdba30fd462f9f794b71eb0e92f417ad50e58aeda094eaad6522'


def dgroup_from_exe(exe):
    """Datensegment aus MTV.EXE der Release-Fassung (LZEXE-gepackt) im Layout des Ports"""
    if exe[:2] != b'MZ' or exe[0x1c:0x20] != b'LZ91':
        sys.exit('MTV.EXE: keine LZEXE-gepackte Release-Fassung')
    img = unlzexe(exe)[0]
    pos = img.find(b'Turbo-C')  # steht bei DS:0004, DGROUP beginnt auf einer Paragraphengrenze
    while pos >= 0 and (pos - 4) % 16:
        pos = img.find(b'Turbo-C', pos + 1)
    if pos < 0:
        sys.exit('MTV.EXE: Datensegment nicht gefunden')
    src = img[pos - 4:]
    dg = bytearray(DGROUP_LEN)
    for dst, s, n in SECTIONS:
        dg[dst:dst + n] = src[s:s + n]
    for dst, data in FILLS:
        dg[dst:dst + len(data)] = data
    if hashlib.sha256(dg).hexdigest() != DGROUP_SHA256:
        sys.exit('MTV.EXE: unbekannte Fassung (Datensegment weicht ab)')
    return bytes(dg)


if __name__ == '__main__':
    exe, out = sys.argv[1:3]
    open(out, 'wb').write(dgroup_from_exe(open(exe, 'rb').read()))

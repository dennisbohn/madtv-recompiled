"""Spieldaten für die Programmdatei vorbereiten (alle Builds betten sie ein).

Aufruf (aus der Projektwurzel): python -I tools/make_bundle.py GAMEDATA_DIR OUT_DIR

GAMEDATA_DIR enthält MTV.EXE, SOURCE1.RSC und SOURCE2.RSC der Release-Fassung von Mad TV.
Erzeugt in OUT_DIR:
  DGROUP.bin   Datensegment im Layout, das der Port erwartet (siehe dgroup_from_exe)
  SOURCE1.RSC  Spieldaten-Archiv OHNE Spielstände (Einträge 209–219 werden aus dem Verzeichnis
               entfernt und ihre Daten mit Nullen überschrieben)
  SOURCE2.RSC  zweites Archiv (Grafik, Intro); wird ebenso geprüft
  *.z          dieselben Dateien gepackt: "MTVZ", Länge entpackt (u32), Deflate-Rohdaten;
               platform_resource entpackt sie beim ersten Zugriff (third_party/tinf)
  bundle.rc    Ressourcenskript (Icon + RCDATA DGROUP/SOURCE1/SOURCE2, gepackt)
  ICON.rgba    Fenstersymbol 64x64 RGBA aus res/madtv.ico (nur Linux, dort ohne Programmsymbol)
  bundle.S     dasselbe für Linux: Tabelle mtv_bundle {Name, Daten, Länge}, Daten per .incbin
Bricht ab, wenn danach noch ein Spielstand-Eintrag enthalten ist.
"""
import hashlib
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from unlzexe import unlzexe  # noqa: E402

RSC_ENTRIES = 600
SAVES = range(0xd1, 0xdc)  # 209.HES (Namen) und 210..219.HES (Spielstände)

# Der Port ist aus dem Code der ersten Fassung von MTV.EXE übersetzt und spricht das Datensegment
# über deren Adressen an (DS_*-Makros). Die Release-Fassung ist neu übersetzt; ihr Datensegment
# enthält dieselben Spieldaten, aber ab DS:057d um einige Byte verschoben. Abschnitte (Adresse im
# Port, Adresse in der Release-Fassung, Länge), ermittelt durch Abgleich beider Segmente:
SECTIONS = [(0x0000, 0x0000, 0x057d), (0x057d, 0x057f, 0x1138), (0x16b6, 0x16b8, 0x00fc),
            (0x17c3, 0x17b7, 0x005d), (0x182c, 0x1820, 0x0115), (0x1941, 0x1961, 0x0009),
            (0x194a, 0x198a, 0x8ef7)]
DGROUP_LEN = 0xa841
# Dazwischen liegen nur Werte der Laufzeitbibliothek, die der Port nicht nutzt (ein Flag, der
# Dateiname "000.HES" der ersten Fassung, Far-Zeiger der Exit-Hooks). Sie werden wie in der ersten
# Fassung gesetzt, damit das Ergebnis Byte für Byte dem bisherigen Datensegment entspricht.
FILLS = [(0x16b5, bytes.fromhex('ff')), (0x17b2, bytes.fromhex("48455300") + b"000.HES" + bytes(6)),
         (0x1820, bytes.fromhex('0f0019350f0019350f001935'))]
DGROUP_SHA256 = '994137498f000727396563fc1a5a272c9902994d413a9691db31877fa98ac1de'


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


def strip_saves(rsc, name):
    rsc = bytearray(rsc)
    if len(rsc) < RSC_ENTRIES * 6:
        sys.exit('%s: zu kurz' % name)
    removed = []
    for n in SAVES:
        off, ln = struct.unpack_from('<IH', rsc, n * 6)
        if ln:
            rsc[off:off + ln] = bytes(ln)  # Spielstand-Daten löschen
            struct.pack_into('<IH', rsc, n * 6, 0, 0)
            removed.append(n)
    for n in SAVES:  # Prüfung
        if struct.unpack_from('<IH', rsc, n * 6)[1]:
            sys.exit('%s: Spielstand-Eintrag %d noch vorhanden' % (name, n))
    return bytes(rsc), removed


def pack(path):
    """Datei als path + '.z' gepackt ablegen (Deflate ohne zlib-Kopf, höchste Stufe)"""
    raw = open(path, 'rb').read()
    c = zlib.compressobj(9, zlib.DEFLATED, -15, 9)
    data = c.compress(raw) + c.flush()
    open(path + '.z', 'wb').write(b'MTVZ' + struct.pack('<I', len(raw)) + data)
    return len(raw), len(data) + 8


def icon_rgba(path, size=64):
    """Bild size x size aus der ICO-Datei (PNG-Einträge von res/make_icon.py) als RGBA-Rohdaten"""
    ico = open(path, 'rb').read()
    for i in range(struct.unpack_from('<H', ico, 4)[0]):
        w, _, _, _, _, _, ln, off = struct.unpack_from('<BBBBHHII', ico, 6 + 16 * i)
        if (w or 256) != size:
            continue
        png, pos, idat = ico[off:off + ln], 8, b''
        while pos < len(png):
            n, t = struct.unpack_from('>I4s', png, pos)
            if t == b'IHDR' and png[pos + 8:pos + 21] != struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0):
                sys.exit('madtv.ico: unerwartetes PNG-Format')
            if t == b'IDAT':
                idat += png[pos + 8:pos + 8 + n]
            pos += 12 + n
        raw, stride, out = zlib.decompress(idat), 1 + size * 4, b''
        for y in range(size):
            if raw[y * stride] != 0:  # make_icon.py schreibt nur Filter 0
                sys.exit('madtv.ico: PNG-Filter %d nicht unterstützt' % raw[y * stride])
            out += raw[y * stride + 1:(y + 1) * stride]
        return out
    sys.exit('madtv.ico: kein %dx%d-Bild' % (size, size))


def main():
    data_dir, out = sys.argv[1:3]
    os.makedirs(out, exist_ok=True)
    dg = dgroup_from_exe(open(os.path.join(data_dir, 'MTV.EXE'), 'rb').read())
    open(os.path.join(out, 'DGROUP.bin'), 'wb').write(dg)
    for k in (1, 2):
        name = 'SOURCE%d.RSC' % k
        data, removed = strip_saves(open(os.path.join(data_dir, name), 'rb').read(), name)
        open(os.path.join(out, name), 'wb').write(data)
        print('%s: %d Byte, Spielstand-Einträge entfernt: %s' % (name, len(data), removed or 'keine'))
    print('DGROUP.bin: %d Byte' % len(dg))
    ico = os.path.abspath('res/madtv.ico').replace('\\', '/')
    with open(os.path.join(out, 'bundle.rc'), 'w', encoding='ascii') as f:
        f.write('// erzeugt von tools/make_bundle.py\n')
        # Programmsymbol (res/make_icon.py); ID 1 nutzt SDL über SDL_HINT_WINDOWS_INTRESOURCE_ICON
        # auch für Fenster und Taskleiste
        f.write('1 ICON "%s"\n' % ico)
        f.write('#include "version.rc"\n')
        for res in ('DGROUP.bin', 'SOURCE1.RSC', 'SOURCE2.RSC'):
            raw, packed = pack(os.path.join(out, res))
            print('%s: %d -> %d Byte gepackt' % (res, raw, packed))
            f.write('%s RCDATA "%s.z"\n' % (res.split('.')[0], res))
    open(os.path.join(out, 'ICON.rgba'), 'wb').write(icon_rgba('res/madtv.ico'))
    pack(os.path.join(out, 'ICON.rgba'))
    # Linux: Daten per .incbin ins Programm; Tabelle in .data.rel.ro (enthält Adressen, PIE)
    names = ('DGROUP', 'SOURCE1', 'SOURCE2', 'ICON')
    files = ('DGROUP.bin', 'SOURCE1.RSC', 'SOURCE2.RSC', 'ICON.rgba')
    with open(os.path.join(out, 'bundle.S'), 'w', encoding='ascii') as f:
        f.write('/* erzeugt von tools/make_bundle.py */\n')
        f.write('\t.section .data.rel.ro,"aw"\n\t.balign 8\n\t.globl mtv_bundle\nmtv_bundle:\n')
        for n in names:
            # .dc.a: zeigergroß (32 wie 64 Bit), passend zu struct BundleEntry in src/platform.c
            f.write('\t.dc.a name_%s, data_%s, end_%s - data_%s\n' % (n, n, n, n))
        f.write('\t.dc.a 0, 0, 0\n\t.section .rodata\n')
        for n, fn in zip(names, files):
            path = os.path.abspath(os.path.join(out, fn + '.z')).replace('\\', '/')
            f.write('name_%s:\t.asciz "%s"\ndata_%s:\t.incbin "%s"\nend_%s:\n' % (n, n, n, path, n))
        f.write('\t.section .note.GNU-stack,"",@progbits\n')


if __name__ == '__main__':
    main()

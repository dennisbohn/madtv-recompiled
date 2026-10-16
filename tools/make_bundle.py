"""Spieldaten und Programmsymbol für die Programmdatei vorbereiten.

Aufruf (aus der Projektwurzel): python -I tools/make_bundle.py GAMEDATA_DIR OUT_DIR [--no-game-data]

Mit --no-game-data wird GAMEDATA_DIR nicht gelesen und nur das Symbol eingebettet; das Spiel liest
SOURCE1.RSC und SOURCE2.RSC dann aus dem Ordner der Programmdatei (Makefile: EMBED=0).

GAMEDATA_DIR enthält SOURCE1.RSC und SOURCE2.RSC der Release-Fassung von Mad TV (die Startwerte
aus dem Datensegment von MTV.EXE stehen in src/game/ds_init.c).
Erzeugt in OUT_DIR:
  SOURCE1.RSC  Spieldaten-Archiv OHNE Spielstände (Einträge 209–219 werden aus dem Verzeichnis
               entfernt und ihre Daten mit Nullen überschrieben)
  SOURCE2.RSC  zweites Archiv (Grafik, Intro); wird ebenso geprüft
  *.z          dieselben Dateien gepackt: "MTVZ", Länge entpackt (u32), Deflate-Rohdaten;
               platform_resource entpackt sie beim ersten Zugriff (third_party/tinf)
  bundle.rc    Ressourcenskript (Icon + RCDATA SOURCE1/SOURCE2, gepackt)
  ICON.rgba    Fenstersymbol 64x64 RGBA aus res/madtv.ico (nur Linux, dort ohne Programmsymbol)
  bundle.S     dasselbe für Linux: Tabelle mtv_bundle {Name, Daten, Länge}, Daten per .incbin
Bricht ab, wenn danach noch ein Spielstand-Eintrag enthalten ist.
"""
import os
import struct
import sys
import zlib

RSC_ENTRIES = 600
SAVES = range(0xd1, 0xdc)  # 209.HES (Namen) und 210..219.HES (Spielstände)


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
    embed = '--no-game-data' not in sys.argv[3:]
    archives = ('SOURCE1.RSC', 'SOURCE2.RSC') if embed else ()
    os.makedirs(out, exist_ok=True)
    for k in (1, 2) if embed else ():
        name = 'SOURCE%d.RSC' % k
        data, removed = strip_saves(open(os.path.join(data_dir, name), 'rb').read(), name)
        open(os.path.join(out, name), 'wb').write(data)
        print('%s: %d Byte, Spielstand-Einträge entfernt: %s' % (name, len(data), removed or 'keine'))
    ico = os.path.abspath('res/madtv.ico').replace('\\', '/')
    with open(os.path.join(out, 'bundle.rc'), 'w', encoding='ascii') as f:
        f.write('// erzeugt von tools/make_bundle.py\n')
        # Programmsymbol (res/make_icon.py); ID 1 nutzt SDL über SDL_HINT_WINDOWS_INTRESOURCE_ICON
        # auch für Fenster und Taskleiste
        f.write('1 ICON "%s"\n' % ico)
        f.write('#include "version.rc"\n')
        for res in archives:
            raw, packed = pack(os.path.join(out, res))
            print('%s: %d -> %d Byte gepackt' % (res, raw, packed))
            f.write('%s RCDATA "%s.z"\n' % (res.split('.')[0], res))
    open(os.path.join(out, 'ICON.rgba'), 'wb').write(icon_rgba('res/madtv.ico'))
    pack(os.path.join(out, 'ICON.rgba'))
    # Linux: Daten per .incbin ins Programm; Tabelle in .data.rel.ro (enthält Adressen, PIE)
    files = archives + ('ICON.rgba',)
    names = tuple(fn.split('.')[0] for fn in files)
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

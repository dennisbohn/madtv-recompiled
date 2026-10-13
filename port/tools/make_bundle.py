"""Spieldaten für die gebündelte Einzel-EXE vorbereiten.

Aufruf (aus port/): python -I tools/make_bundle.py MTV_EXE_V1 V2_DIR OUT_DIR

Erzeugt in OUT_DIR:
  DGROUP.bin   Datensegment aus der MTV.EXE der Originalfassung (der Port ist aus deren Code
               übersetzt und erwartet dessen Adressen; inhaltlich gleich wie in Fassung 2)
  SOURCE1.RSC  Spieldaten-Archiv der überarbeiteten Fassung OHNE Spielstände (Einträge 209–219
               werden aus dem Verzeichnis entfernt und ihre Daten mit Nullen überschrieben)
  SOURCE2.RSC  zweites Archiv (Grafik, Intro); wird ebenso geprüft
  *.z          dieselben Dateien gepackt: "MTVZ", Länge entpackt (u32), Deflate-Rohdaten;
               platform_resource entpackt sie beim ersten Zugriff (third_party/tinf)
  bundle.rc    Ressourcenskript (Icon + RCDATA DGROUP/SOURCE1/SOURCE2, gepackt)
  ICON.rgba    Fenstersymbol 64x64 RGBA aus res/madtv.ico (nur Linux, dort ohne Programmsymbol)
  bundle.S     dasselbe für Linux: Tabelle mtv_bundle {Name, Daten, Länge}, Daten per .incbin
Bricht ab, wenn danach noch ein Spielstand-Eintrag enthalten ist.
"""
import os
import struct
import sys
import zlib

DGROUP_PARA = 0x35d4       # Lage von DGROUP im Ladeabbild (siehe src/dgroup.c)
RSC_ENTRIES = 600
SAVES = range(0xd1, 0xdc)  # 209.HES (Namen) und 210..219.HES (Spielstände)


def dgroup_ok(dg):
    return dg[4:11] == b'Turbo-C' and 0x2000 <= len(dg) <= 0x10000


def extract_dgroup(exe):
    """Datensegment aus MTV.EXE der Originalfassung, aus einer Release-EXE dieses Ports (eingebettete
    Ressource DGROUP, gepackt) oder aus einem bereits herausgelösten Abbild (DGROUP.bin)"""
    if dgroup_ok(exe):
        return exe
    if exe[:2] != b'MZ':
        sys.exit('MTV.EXE: keine MZ-Datei')
    pe = struct.unpack_from('<I', exe, 0x3c)[0] if len(exe) > 0x40 else 0
    if exe[pe:pe + 4] == b'PE\0\0':  # Release-EXE: gepackte Ressourcen ("MTVZ") durchsuchen
        pos = exe.find(b'MTVZ')
        while pos >= 0:
            raw = struct.unpack_from('<I', exe, pos + 4)[0]
            try:
                dg = zlib.decompressobj(-15).decompress(exe[pos + 8:], raw)
            except zlib.error:
                dg = b''
            if len(dg) == raw and dgroup_ok(dg):
                return dg
            pos = exe.find(b'MTVZ', pos + 4)
        sys.exit('Release-EXE: eingebettetes Datensegment nicht gefunden')
    last, pages, _, head_para = struct.unpack_from('<4H', exe, 2)
    end = (pages - 1) * 512 + last if last else pages * 512
    dg = exe[head_para * 16 + DGROUP_PARA * 16:end]
    if dg[4:11] != b'Turbo-C' or not 0x2000 <= len(dg) <= 0x10000:
        sys.exit('MTV.EXE: Datensegment nicht gefunden (falsche Fassung? Erwartet wird die Originalfassung)')
    return dg


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
    exe_path, v2_dir, out = sys.argv[1:4]
    os.makedirs(out, exist_ok=True)
    dg = extract_dgroup(open(exe_path, 'rb').read())
    open(os.path.join(out, 'DGROUP.bin'), 'wb').write(dg)
    for k in (1, 2):
        name = 'SOURCE%d.RSC' % k
        data, removed = strip_saves(open(os.path.join(v2_dir, name), 'rb').read(), name)
        open(os.path.join(out, name), 'wb').write(data)
        print('%s: %d Byte, Spielstand-Einträge entfernt: %s' % (name, len(data), removed or 'keine'))
    print('DGROUP.bin: %d Byte' % len(dg))
    ico = os.path.abspath('res/madtv.ico').replace('\\', '/')
    with open(os.path.join(out, 'bundle.rc'), 'w', encoding='ascii') as f:
        f.write('// erzeugt von tools/make_bundle.py\n')
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
        f.write('\t.section .data.rel.ro,"aw"\n\t.balign 4\n\t.globl mtv_bundle\nmtv_bundle:\n')
        for n in names:
            f.write('\t.long name_%s, data_%s, end_%s - data_%s\n' % (n, n, n, n))
        f.write('\t.long 0, 0, 0\n\t.section .rodata\n')
        for n, fn in zip(names, files):
            path = os.path.abspath(os.path.join(out, fn + '.z')).replace('\\', '/')
            f.write('name_%s:\t.asciz "%s"\ndata_%s:\t.incbin "%s"\nend_%s:\n' % (n, n, n, path, n))
        f.write('\t.section .note.GNU-stack,"",@progbits\n')


if __name__ == '__main__':
    main()

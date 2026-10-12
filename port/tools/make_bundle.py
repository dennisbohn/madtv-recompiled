"""Spieldaten für die gebündelte Einzel-EXE vorbereiten.

Aufruf (aus port/): python -I tools/make_bundle.py MTV_EXE_V1 V2_DIR OUT_DIR

Erzeugt in OUT_DIR:
  DGROUP.bin   Datensegment aus der MTV.EXE der Originalfassung (der Port ist aus deren Code
               übersetzt und erwartet dessen Adressen; inhaltlich gleich wie in Fassung 2)
  SOURCE1.RSC  Spieldaten-Archiv der überarbeiteten Fassung OHNE Spielstände (Einträge 209–219
               werden aus dem Verzeichnis entfernt und ihre Daten mit Nullen überschrieben)
  SOURCE2.RSC  zweites Archiv (Grafik, Intro); wird ebenso geprüft
  bundle.rc    Ressourcenskript (Icon + RCDATA DGROUP/SOURCE1/SOURCE2)
Bricht ab, wenn danach noch ein Spielstand-Eintrag enthalten ist.
"""
import os
import struct
import sys

DGROUP_PARA = 0x35d4       # Lage von DGROUP im Ladeabbild (siehe src/dgroup.c)
RSC_ENTRIES = 600
SAVES = range(0xd1, 0xdc)  # 209.HES (Namen) und 210..219.HES (Spielstände)


def extract_dgroup(exe):
    if exe[:2] != b'MZ':
        sys.exit('MTV.EXE: keine MZ-Datei')
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
            f.write('%s RCDATA "%s"\n' % (res.split('.')[0], res))


if __name__ == '__main__':
    main()

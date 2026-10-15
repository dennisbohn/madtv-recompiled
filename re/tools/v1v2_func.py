"""Eine Funktion der ersten Fassung und ihr Gegenstück in der Release-Fassung gegenüberstellen.

Aufruf (Projektwurzel, capstone im PYTHONPATH): python re/tools/v1v2_func.py NAME|SEG:OFF [--asm]
Das Gegenstück kommt aus re/notes/v1v2_map.tsv (re/tools/v1v2_map.py). Gezeigt werden die
Unterschiede der inhaltlichen Merkmale (Datenzugriffe, Konstanten, Aufrufe), mit --asm zusätzlich
beide Listings.
"""
import difflib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import v1v2_map as M  # noqa: E402


def main():
    key = sys.argv[1]
    row = None
    for line in open('re/notes/v1v2_map.tsv', encoding='utf-8'):
        p = line.rstrip('\n').split('\t')
        if not line.startswith('#') and key in (p[0], p[2]):
            row = p
    if not row or not row[1]:
        sys.exit('%s: kein Gegenstück in re/notes/v1v2_map.tsv' % key)
    v1, rel1 = M.load_v1('re/v1/MTV.EXE')
    v2, rel2 = M.load_v2('gamedata/MTV.EXE')
    a1 = [int(x, 16) for x in row[0].split(':')]
    a2 = [int(x, 16) for x in row[1].split(':')]
    f1 = M.Func(v1, a1[0] - 0x1000, a1[1], lambda x: x, rel1)
    f2 = M.Func(v2, a2[0] - 0x1000, a2[1], M.ds_v2_to_v1, rel2)
    print('%s  Beta %s  Release %s  %s %s' % (row[2], row[0], row[1], row[3], row[4]))
    for line in difflib.unified_diff(f1.tokens, f2.tokens, 'Beta', 'Release', lineterm='', n=2):
        print(line)
    if '--asm' in sys.argv:
        for title, f in (('Beta', f1), ('Release', f2)):
            print('\n== %s' % title)
            for a in sorted(f.ins):
                print('%04x  %-6s %s' % (a, f.ins[a].mnemonic, f.ins[a].op_str))


if __name__ == '__main__':
    main()

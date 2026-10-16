"""Eine Funktion der ersten Fassung und ihr Gegenstück in der Release-Fassung gegenüberstellen.

Aufruf (Projektwurzel, capstone im PYTHONPATH): python re/tools/v1v2_func.py NAME|SEG:OFF [--ctx|--asm]
Das Gegenstück kommt aus re/notes/v1v2_map.tsv (re/tools/v1v2_map.py). Gezeigt werden die
Unterschiede der inhaltlichen Merkmale (Datenzugriffe, Konstanten, Aufrufe), mit --ctx je
Unterschied die Befehle beider Fassungen an dieser Stelle, mit --asm beide Listings.
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
    f2.tokens = M.resolve(f2.tokens, f1.tokens)
    print('%s  Beta %s  Release %s  %s %s' % (row[2], row[0], row[1], row[3], row[4]))
    if '--ctx' in sys.argv:
        sm = difflib.SequenceMatcher(None, f1.tokens, f2.tokens, autojunk=False)
        for tag, i1, i2, j1, j2 in sm.get_opcodes():
            if tag == 'equal':
                continue
            print('\n@@ %s  Beta %s  Release %s' % (tag, ' '.join(f1.tokens[i1:i2]), ' '.join(f2.tokens[j1:j2])))
            for title, f, k1, k2 in (('Beta', f1, i1, i2), ('Release', f2, j1, j2)):
                lo = f.token_addr[max(k1 - 1, 0)]
                hi = f.token_addr[min(k2, len(f.token_addr) - 1)]
                addrs = sorted(a for a in f.ins if lo <= a <= hi)
                print('  -- %s' % title)
                for a in addrs[:40]:
                    print('  %04x  %-6s %s' % (a, f.ins[a].mnemonic, f.ins[a].op_str))
    else:
        for line in difflib.unified_diff(f1.tokens, f2.tokens, 'Beta', 'Release', lineterm='', n=2):
            print(line)
    if '--asm' in sys.argv:
        for title, f in (('Beta', f1), ('Release', f2)):
            print('\n== %s' % title)
            for a in sorted(f.ins):
                print('%04x  %-6s %s' % (a, f.ins[a].mnemonic, f.ins[a].op_str))


if __name__ == '__main__':
    main()

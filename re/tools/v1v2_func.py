"""Eine Funktion von Version 1 in Version 2 suchen und beide normalisiert gegenüberstellen.

Aufruf: python re/tools/v1v2_func.py SEG:OFF [--asm]
Zeigt den Unterschied der normalisierten Befehlsfolgen (difflib) und mit --asm beide Listings.
"""
import difflib
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from unlzexe import unlzexe  # noqa: E402
from v1v2_compare import load_v1, md, tokens  # noqa: E402


def listing(img, lin, n=400):
    out = []
    for ins in md.disasm(img[lin:lin + 4000], 0):
        out.append('%04x  %-6s %s' % (ins.address, ins.mnemonic, ins.op_str))
        if ins.mnemonic in ('ret', 'retf') or len(out) >= n:
            break
    return out


def main():
    s, o = (int(x, 16) for x in sys.argv[1].split(':'))
    v1 = load_v1('Gamefiles/MTV.EXE')
    v2, _, _ = unlzexe(open('Gamefiles2/MTV.EXE', 'rb').read())
    lin = (s - 0x1000) * 16 + o
    head = tokens(v1, lin, 12)
    full1 = tokens(v1, lin, 4000)
    cands = [m.start() for m in re.finditer(b'\x55\x8b\xec', v2) if tokens(v2, m.start(), 12) == head]
    if not cands:
        print('kein Gegenstück gefunden')
        return
    best = max(cands, key=lambda c: difflib.SequenceMatcher(None, full1, tokens(v2, c, 4000)).ratio())
    full2 = tokens(v2, best, 4000)
    print('Version 2: lineare Adresse %x (Seg %04x:%04x), Ähnlichkeit %.2f, %d/%d Befehle' % (
        best, 0x1000 + best // 16, best % 16, difflib.SequenceMatcher(None, full1, full2).ratio(), len(full1), len(full2)))
    for line in difflib.unified_diff(full1, full2, 'v1', 'v2', n=2, lineterm=''):
        print(line)
    if '--asm' in sys.argv:
        print('\n--- Version 1 ---')
        print('\n'.join(listing(v1, lin)))
        print('\n--- Version 2 ---')
        print('\n'.join(listing(v2, best)))


if __name__ == '__main__':
    main()

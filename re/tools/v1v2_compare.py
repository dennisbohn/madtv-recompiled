"""Vergleich der Programmstände MTV.EXE Version 1 (re/v1, Beta) und Version 2 (gamedata, LZEXE).

Aufruf (Projektwurzel, capstone im PYTHONPATH): python re/tools/v1v2_compare.py [--list]

Version 2 ist mit einem neueren Borland-Compiler neu übersetzt (z. B. "inc sp; inc sp" statt
"pop cx" zum Aufräumen nach Aufrufen, andere Segment- und Datenadressen). Verglichen wird daher
auf Befehlsebene: Mnemonics und Operandenarten, Adressen/Konstanten ausgeblendet, Compiler-
Eigenheiten vereinheitlicht. Für jede Funktion aus re/notes/sigs/*.tsv (Version 1) wird der
Anfang in Version 2 gesucht und anschließend die ganze Funktion verglichen.
"""
import glob
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tools'))
from unlzexe import unlzexe  # noqa: E402

import capstone  # noqa: E402
from capstone import x86  # noqa: E402

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
md.detail = True


def load_v1(path):
    d = open(path, 'rb').read()
    h = struct.unpack_from('<14H', d, 0)
    end = (h[2] - 1) * 512 + h[1] if h[1] else h[2] * 512
    return d[h[4] * 16:end]


def tokens(img, lin, maxlen):
    """Normalisierte Befehlsfolge ab lin bis zum ersten ret/retf (höchstens maxlen Befehle)."""
    out = []
    for ins in md.disasm(img[lin:lin + 6000], 0):
        m = ins.mnemonic
        if m in ('ret', 'retf'):
            out.append('ret')
            break
        ops = []
        for op in ins.operands:
            if op.type == x86.X86_OP_REG:
                ops.append(ins.reg_name(op.reg))
            elif op.type == x86.X86_OP_IMM:
                ops.append('i')
            elif op.type == x86.X86_OP_MEM:
                base = ins.reg_name(op.mem.base) if op.mem.base else ''
                idx = ins.reg_name(op.mem.index) if op.mem.index else ''
                # Stackrahmen-Offsets sind aussagekräftig, globale Adressen nicht
                disp = str(op.mem.disp) if base == 'bp' else ''
                ops.append('[%s%s%s]' % (base, '+' + idx if idx else '', disp))
        if m.startswith(('call', 'lcall')):
            out.append('call')
        elif m.startswith('j') or m.startswith('loop'):
            out.append(m)
        else:
            out.append(m + ' ' + ','.join(ops))
        if len(out) >= maxlen:
            break
    # Compiler-Eigenheiten vereinheitlichen
    norm = []
    i = 0
    while i < len(out):
        t = out[i]
        if t == 'inc sp' and i + 1 < len(out) and out[i + 1] == 'inc sp':
            norm.append('pop2')
            i += 2
            continue
        if t in ('pop cx', 'add sp,i') and norm and norm[-1] == 'call':
            norm.append('pop2' if t == 'pop cx' else 'addsp')
            i += 1
            continue
        if t == 'inc sp,' or t == 'inc sp':
            t = 'inc sp'
        norm.append(t)
        i += 1
    return norm


def main():
    v1 = load_v1('re/v1/MTV.EXE')
    v2, _, _ = unlzexe(open('gamedata/MTV.EXE', 'rb').read())
    funcs = []
    for f in sorted(glob.glob('re/notes/sigs/*.tsv')):
        for line in open(f, encoding='utf-8'):
            parts = line.rstrip('\n').split('\t')
            if re.fullmatch(r'[0-9a-f]{4}:[0-9a-f]{4}', parts[0]):
                s, o = (int(x, 16) for x in parts[0].split(':'))
                funcs.append((s, o, parts[1] if len(parts) > 1 else ''))

    # Kandidaten in Version 2: alle "push bp; mov bp,sp"-Stellen und Funktionsanfänge allgemein
    starts = [m.start() for m in re.finditer(b'\x55\x8b\xec', v2)]
    key_len = 12
    index = {}
    for lin in starts:
        t = tokens(v2, lin, key_len)
        index.setdefault(tuple(t), []).append(lin)

    same = changed = missing = 0
    rows = []
    for s, o, name in funcs:
        lin = (s - 0x1000) * 16 + o
        head = tuple(tokens(v1, lin, key_len))
        cands = index.get(head, [])
        if not cands:
            missing += 1
            rows.append(('fehlt', s, o, name))
            continue
        full1 = tokens(v1, lin, 4000)
        best = None
        for c in cands:
            full2 = tokens(v2, c, 4000)
            if full1 == full2:
                best = ('gleich', c)
                break
            if best is None:
                best = ('anders', c)
        if best[0] == 'gleich':
            same += 1
        else:
            changed += 1
        rows.append((best[0], s, o, name))
    print('Funktionen Version 1: %d' % len(funcs))
    print('  logisch gleich in Version 2:   %d' % same)
    print('  Anfang gefunden, Rest anders:  %d' % changed)
    print('  nicht zugeordnet:              %d' % missing)
    if '--list' in sys.argv:
        for st, s, o, name in rows:
            if st != 'gleich':
                print('%-6s %04x:%04x %s' % (st, s, o, name))


if __name__ == '__main__':
    main()

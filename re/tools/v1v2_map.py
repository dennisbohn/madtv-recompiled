"""Funktionen der ersten Fassung von MTV.EXE (re/v1, Beta) der Release-Fassung (gamedata) zuordnen.

Aufruf (Projektwurzel, capstone im PYTHONPATH): python re/tools/v1v2_map.py [--diff]

Die Release-Fassung ist mit einem neueren Borland-Compiler übersetzt: andere Befehlsauswahl und
Registerwahl, Code und Daten an anderen Adressen. Verglichen werden daher nur inhaltliche
Merkmale in Adressreihenfolge: Zugriffe auf das Datensegment (Adressen der Release-Fassung in die
der ersten Fassung umgerechnet, VAR_SHIFT), Konstanten außer 0 und Aufrufe.

Zuordnung: main liegt in beiden Fassungen bei 0054:0005. Von einem Paar aus werden Aufrufe in
übereinstimmenden Abschnitten gepaart. Danach werden noch fehlende Funktionen (z. B. nur über
Zeiger aufgerufen) unter allen Funktionsanfängen der Release-Fassung nach Ähnlichkeit gesucht.
Paare mit weniger als 50 % Übereinstimmung werden verworfen.

Ausgabe: re/notes/v1v2_map.tsv  (Adresse Beta, Adresse Release, Name, Ergebnis, Ähnlichkeit)
  gleich  Merkmale identisch
  anders  Merkmale weichen ab (Ähnlichkeit 0..1); --diff zeigt die Unterschiede,
          re/tools/v1v2_func.py NAME eine einzelne Funktion
  fehlt   kein Gegenstück gefunden
"""
import difflib
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tools'))
from unlzexe import unlzexe  # noqa: E402

import capstone  # noqa: E402
from capstone import x86  # noqa: E402

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
md.detail = True

V1_DGROUP_PARA = 0x35d4
V2_DGROUP_PARA = 0x39d4


def load_v1(path):
    """Ladeabbild und Menge der relozierten Stellen (lineare Adressen) der ungepackten Beta"""
    d = open(path, 'rb').read()
    last, pages, nrel, head = (int.from_bytes(d[i:i + 2], 'little') for i in (2, 4, 6, 8))
    reloff = int.from_bytes(d[0x18:0x1a], 'little')
    end = (pages - 1) * 512 + last if last else pages * 512
    rel = set()
    for i in range(nrel):
        o, s = (int.from_bytes(d[reloff + 4 * i + k:reloff + 4 * i + k + 2], 'little') for k in (0, 2))
        rel.add(s * 16 + o)
    return d[head * 16:end], rel


def load_v2(path):
    img, _, rel = unlzexe(open(path, 'rb').read())
    return img, {s * 16 + o for o, s in rel}


# Lage der Variablen: Verschiebung Release gegenüber erster Fassung je Bereich (Adressen der ersten
# Fassung), ermittelt aus den Datenzugriffen zugeordneter Funktionen. Im genullten Bereich ab etwa
# 0x1950 (Variablen ohne Startwert) ist sie +0x20; tools/make_bundle.py ordnet dort nur Nullen zu.
VAR_SHIFT = [(0x0000, 0x057d, 0), (0x057d, 0x17c4, 2), (0x17c4, 0x1941, -0xc), (0x1941, 0xa540, 0x20)]


def ds_v2_to_v1(x):
    """Datensegment-Offset der Release-Fassung -> erste Fassung"""
    for lo, hi, d in VAR_SHIFT:
        if lo <= x - d < hi:
            return x - d
    return x


class Func:
    """Funktion ab seg:off (Segment relativ zum Ladeabbild): Befehle in Adressreihenfolge"""

    def __init__(self, img, seg, off, ds_map, rel=frozenset(), imm_map=None):
        self.seg, self.off, self.rel = seg, off, rel
        self.imm_map = imm_map or ds_map
        self.ins = {}
        self.calls = {}  # Befehlsadresse -> ('far', seg, off) | ('near', off)
        todo = [off]
        base = seg * 16
        while todo:
            o = todo.pop()
            while o not in self.ins and 0 <= o < 0x10000 and base + o < len(img):
                code = img[base + o:base + o + 16]
                ins = next(md.disasm(code, o), None)
                if ins is None:
                    break
                self.ins[o] = ins
                m = ins.mnemonic
                if m in ('ret', 'retf', 'iret', 'hlt'):
                    break
                if m == 'lcall' and len(ins.operands) == 2:
                    self.calls[o] = ('far', ins.operands[0].imm, ins.operands[1].imm)
                elif m == 'call' and ins.operands[0].type == x86.X86_OP_IMM:
                    self.calls[o] = ('near', ins.operands[0].imm & 0xffff)
                if m.startswith('j') or m.startswith('loop'):
                    op = ins.operands[0]
                    if op.type == x86.X86_OP_IMM:
                        todo.append(op.imm & 0xffff)
                    elif m == 'jmp':
                        todo.extend(self.switch_targets(img, ins))
                    if m == 'jmp' or m == 'ljmp':
                        break
                o = (o + ins.size) & 0xffff
        self.tokens, self.token_addr = self.normalize(ds_map)

    def switch_targets(self, img, ins):
        """jmp cs:[bx+Tabelle]: Grenze aus dem vorangehenden "cmp reg, n" """
        op = ins.operands[0]
        if op.type != x86.X86_OP_MEM or ins.reg_name(op.mem.segment) != 'cs':
            return []
        prev = sorted(a for a in self.ins if a < ins.address)[-6:]
        n = None
        for a in reversed(prev):
            p = self.ins[a]
            if p.mnemonic == 'cmp' and len(p.operands) == 2 and p.operands[1].type == x86.X86_OP_IMM:
                n = p.operands[1].imm + 1
                break
        if n is None or n > 256:
            return []
        t = self.seg * 16 + (op.mem.disp & 0xffff)
        return [int.from_bytes(img[t + 2 * i:t + 2 * i + 2], 'little') for i in range(n)]

    def normalize(self, ds_map):
        """Inhaltliche Merkmale in Adressreihenfolge: Datensegment-Zugriffe (D), Konstanten außer 0 (I),
        Aufrufe (call). Registerwahl, Stapelrahmen, Sprünge und Stapelaufräumen nach Aufrufen
        hängen vom Compiler ab und entfallen."""
        toks, addrs = [], []
        for a in sorted(self.ins):
            ins = self.ins[a]
            m = ins.mnemonic
            if m in ('call', 'lcall'):
                toks.append('call' if a in self.calls else 'call*')
                addrs.append(a)
                continue
            if m.startswith('j') or m.startswith('loop'):
                continue
            regs = [ins.reg_name(op.reg) for op in ins.operands if op.type == x86.X86_OP_REG]
            if 'sp' in regs or 'bp' in regs and m in ('mov', 'push', 'pop'):
                continue  # Stapelrahmen und -aufräumen
            for op in ins.operands:
                if op.type == x86.X86_OP_IMM:
                    lin = self.seg * 16 + a
                    if any(lin + k in self.rel for k in range(ins.size)):
                        toks.append('Iseg')  # Segmentwert, hängt von der Lage im Programm ab
                    elif op.imm & 0xffff:
                        toks.append('I%x' % self.imm_map(op.imm & 0xffff))
                    else:
                        continue  # 0 setzt der Compiler mal als Konstante, mal per "xor r, r"
                    addrs.append(a)
                elif op.type == x86.X86_OP_MEM:
                    seg = ins.reg_name(op.mem.segment) if op.mem.segment else ''
                    base = ins.reg_name(op.mem.base) if op.mem.base else ''
                    if base == 'bp' or seg not in ('', 'ds'):
                        continue
                    toks.append('D%x' % ds_map(op.mem.disp & 0xffff))
                    addrs.append(a)
        return toks, addrs


def v1_functions():
    funcs = {}
    for f in sorted(glob.glob('re/notes/sigs/*.tsv')):
        for line in open(f, encoding='utf-8'):
            parts = line.rstrip('\n').split('\t')
            if re.fullmatch(r'[0-9a-f]{4}:[0-9a-f]{4}', parts[0]):
                s, o = (int(x, 16) for x in parts[0].split(':'))
                funcs[(s - 0x1000, o)] = parts[1] if len(parts) > 1 else ''
    return funcs


def main():
    v1, rel1 = load_v1('re/v1/MTV.EXE')
    v2, rel2 = load_v2('gamedata/MTV.EXE')
    names = v1_functions()
    pairs = {}  # (seg1, off1) -> (seg2, off2)
    results = {}
    used2 = set()  # bereits vergebene Funktionen der Release-Fassung
    v2_targets = set()  # alle in der Release-Fassung gesehenen Aufrufziele
    f1_cache, f2_cache = {}, {}

    def func1(k):
        if k not in f1_cache:
            f1_cache[k] = Func(v1, k[0], k[1], lambda x: x, rel1)
        return f1_cache[k]

    # Zahlenwerte sind teils Adressen (Tabellenanfänge), teils Konstanten (9999, 10000 ...). Umgerechnet
    # wird, wenn nur der umgerechnete Wert in der ersten Fassung vorkommt; kommen beide vor, entscheidet,
    # ob der umgerechnete Wert dort als Variable angesprochen wird.
    v1_vars, v1_vals = set(), set()
    for k in names:
        for t in func1(k).tokens:
            if t[0] in 'DI' and t != 'Iseg':
                v1_vals.add(int(t[1:], 16))
                if t[0] == 'D':
                    v1_vars.add(int(t[1:], 16))

    def imm_v2_to_v1(x):
        m = ds_v2_to_v1(x)
        if m == x or (m not in v1_vals and x in v1_vals):
            return x
        if x not in v1_vals or m in v1_vars:
            return m
        return x

    def func2(k):
        if k not in f2_cache:
            f2_cache[k] = Func(v2, k[0], k[1], ds_v2_to_v1, rel2, imm_v2_to_v1)
            for c in f2_cache[k].calls.values():
                v2_targets.add((c[1], c[2]) if c[0] == 'far' else (k[0], c[1]))
        return f2_cache[k]

    def propagate(queue):
        while queue:
            k1, k2 = queue.pop(0)
            if k1 in pairs or k2 in used2:
                continue
            f1, f2 = func1(k1), func2(k2)
            sm = difflib.SequenceMatcher(None, f1.tokens, f2.tokens, autojunk=False)
            if sm.ratio() < 0.5 and max(len(f1.tokens), len(f2.tokens)) >= 2:
                continue  # zu verschieden: vermutlich falsch gepaart
            pairs[k1] = k2
            used2.add(k2)
            results[k1] = (f1.tokens == f2.tokens, sm.ratio(), f1, f2, sm)
            # Aufrufe in übereinstimmenden Abschnitten paaren
            for tag, i1, i2, j1, j2 in sm.get_opcodes():
                # nur Abschnitte mit Kontext: "call" allein passt auf jeden anderen Aufruf
                if tag != 'equal' or sum(1 for t in f1.tokens[i1:i2] if t != 'call') < 2:
                    continue
                for d in range(i2 - i1):
                    if f1.tokens[i1 + d] != 'call' or f2.tokens[j1 + d] != 'call':
                        continue
                    c1 = f1.calls[f1.token_addr[i1 + d]]
                    c2 = f2.calls[f2.token_addr[j1 + d]]
                    if c1[0] != c2[0]:
                        continue
                    t1 = (c1[1], c1[2]) if c1[0] == 'far' else (k1[0], c1[1])
                    t2 = (c2[1], c2[2]) if c2[0] == 'far' else (k2[0], c2[1])
                    if t1 not in pairs:
                        queue.append((t1, t2))

    propagate([((0x54, 5), (0x54, 5))])

    # Zweiter Durchgang: nicht erreichte Funktionen (nur über Zeiger aufgerufen o. ä.) unter allen
    # Funktionsanfängen der Release-Fassung nach Ähnlichkeit suchen, dann wieder über Aufrufe paaren
    segs = sorted({s for s, _ in v2_targets})
    for m in re.finditer(bytes.fromhex("558bec"), v2):  # push bp; mov bp, sp
        s = max((x for x in segs if x * 16 <= m.start()), default=None)
        if s is not None and m.start() - s * 16 < 0x10000:
            v2_targets.add((s, m.start() - s * 16))
    while True:
        cands = [k for k in sorted(v2_targets) if k not in used2]
        new = []
        for k1 in sorted(k for k in names if k not in pairs):
            f1 = func1(k1)
            if len(f1.tokens) < 4:
                continue  # zu wenig Merkmale für eine eindeutige Zuordnung
            scored = []
            for k2 in cands:
                f2 = func2(k2)
                sm = difflib.SequenceMatcher(None, f1.tokens, f2.tokens, autojunk=False)
                if sm.real_quick_ratio() < 0.7 or sm.quick_ratio() < 0.7:
                    continue
                scored.append((sm.ratio(), k2))
            scored.sort(reverse=True)
            if scored and scored[0][0] >= 0.75 and (len(scored) == 1 or scored[0][0] - scored[1][0] >= 0.1):
                new.append((k1, scored[0][1]))
        new = [(k1, k2) for k1, k2 in new if [n[1] for n in new].count(k2) == 1]
        if not new:
            break
        propagate(new)

    rows = []
    for k1 in sorted(set(names) | set(pairs)):
        a1 = '%04x:%04x' % (k1[0] + 0x1000, k1[1])
        if k1 in pairs:
            k2 = pairs[k1]
            same, ratio, f1, f2, sm = results[k1]
            rows.append((a1, '%04x:%04x' % (k2[0] + 0x1000, k2[1]), names.get(k1, '?'),
                         'gleich' if same else 'anders', '%.2f' % ratio))
        else:
            rows.append((a1, '', names.get(k1, '?'), 'fehlt', ''))
    with open('re/notes/v1v2_map.tsv', 'w', encoding='utf-8', newline='\n') as f:
        f.write('# Beta\tRelease\tName\tErgebnis\tÄhnlichkeit (re/tools/v1v2_map.py)\n')
        for r in rows:
            f.write('\t'.join(r) + '\n')
    known = [r for r in rows if r[2] != '?']
    for st in ('gleich', 'anders', 'fehlt'):
        print('%-7s %d' % (st, sum(1 for r in known if r[3] == st)))
    print('zusätzlich erreicht (nicht in sigs): %d' % sum(1 for r in rows if r[2] == '?'))
    if '--diff' in sys.argv:
        for k1, (same, ratio, f1, f2, sm) in sorted(results.items()):
            if same or k1 not in names:
                continue
            print('\n== %04x:%04x %s  %.2f' % (k1[0] + 0x1000, k1[1], names[k1], ratio))
            for tag, i1, i2, j1, j2 in sm.get_opcodes():
                if tag == 'equal':
                    continue
                for t in f1.tokens[i1:i2]:
                    print('  - ' + t)
                for t in f2.tokens[j1:j2]:
                    print('  + ' + t)


if __name__ == '__main__':
    main()

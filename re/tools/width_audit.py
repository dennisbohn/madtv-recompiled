"""Zugriffsbreiten auf feste DGROUP-Adressen: Original (MTV.EXE) gegen Port-Code vergleichen.

Aufruf: python width_audit.py            (Projektwurzel als Arbeitsverzeichnis, capstone im PYTHONPATH)

Original: jede Funktion aus re/notes/sigs/*.tsv wird bis zum Beginn der nächsten Funktion desselben
Segments disassembliert; gesammelt werden direkte Speicheroperanden [disp] ohne Basis-/Indexregister
und ohne ES-Präfix (also DS-relativ) mit ihrer Operandengröße.
Port: DS_I8/U8 (1), DS_I16/U16 (2), DS_I32/U32/DS_PTR (4) mit konstanter Adresse.
Gemeldet wird, wenn der Port breiter zugreift als jeder Zugriff des Originals auf diese Adresse
(Gefahr, Nachbarvariablen zu überschreiben) oder schmaler (Wert wird abgeschnitten).
"""
import glob
import os
import re
import struct
import sys
from collections import defaultdict

import capstone
from capstone import x86

ROOT = os.getcwd()
EXE = open(os.path.join(ROOT, "Gamefiles", "MTV.EXE"), "rb").read()
HDR = struct.unpack_from("<H", EXE, 8)[0] * 16

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
md.detail = True

funcs = defaultdict(list)
for f in glob.glob(os.path.join(ROOT, "re", "notes", "sigs", "*.tsv")):
    for line in open(f, encoding="utf-8"):
        a = line.split("\t")[0]
        if re.fullmatch(r"[0-9a-f]{4}:[0-9a-f]{4}", a):
            s, o = a.split(":")
            funcs[int(s, 16)].append(int(o, 16))

orig = defaultdict(set)  # addr -> Breiten
orig_idx = defaultdict(set)  # disp bei [reg+disp] -> Breiten
where = defaultdict(set)
for seg, offs in funcs.items():
    offs = sorted(set(offs))
    for i, off in enumerate(offs):
        end = offs[i + 1] if i + 1 < len(offs) else off + 0x2000
        lin = (seg - 0x1000) * 16 + off
        code = EXE[HDR + lin: HDR + lin + (end - off)]
        for ins in md.disasm(code, off):
            for op in ins.operands:
                if op.type != x86.X86_OP_MEM:
                    continue
                m = op.mem
                if m.base != 0 or m.index != 0:
                    if m.segment in (0, x86.X86_REG_DS) and ins.mnemonic != "lea" and (m.disp & 0xffff) >= 0x100:
                        size = 4 if ins.mnemonic in ("les", "lds") else op.size
                        orig_idx[m.disp & 0xffff].add(size)
                    continue
                if m.segment not in (0, x86.X86_REG_DS):
                    continue
                if ins.mnemonic in ("lea",):
                    continue
                addr = m.disp & 0xffff
                size = op.size
                if ins.mnemonic in ("les", "lds"):
                    size = 4
                orig[addr].add(size)
                where[addr].add("%04x:%04x" % (seg, off))

port = defaultdict(list)
pat = re.compile(r"DS_(I8|U8|I16|U16|I32|U32|PTR)\((?:[A-Za-z_ ]+,\s*)?0x([0-9a-fA-F]+)\)")
W = {"I8": 1, "U8": 1, "I16": 2, "U16": 2, "I32": 4, "U32": 4, "PTR": 4}
for f in glob.glob(os.path.join(ROOT, "port", "src", "game", "*.c")):
    for n, line in enumerate(open(f, encoding="utf-8"), 1):
        for m in pat.finditer(line):
            port[int(m.group(2), 16)].append((W[m.group(1)], os.path.basename(f), n, line.strip()[:110]))

bad = 0
susp = []
for addr in sorted(port):
    if addr not in orig:
        continue
    o = orig[addr]
    eff = set(o)
    hi = addr + 2
    if 2 in o and 2 in orig.get(hi, set()):
        # 32-Bit-Wert bzw. Far-Pointer: im 16-Bit-Code zwei Wortzugriffe. Verdächtig, wenn das
        # obere Wort in Funktionen benutzt wird, die das untere nicht anfassen (eigene Variable).
        alone = where.get(hi, set()) - where.get(addr, set())
        if alone:
            susp.append((addr, sorted(alone)))
        eff.add(4)
    for w, f, n, txt in port[addr]:
        if w > max(eff) or w < min(eff):
            bad += 1
            kind = "BREITER" if w > max(eff) else "schmaler"
            print("%s 0x%04x port=%d orig=%s  %s:%d  %s" % (kind, addr, w, sorted(o), f, n, txt))
for addr, fns in susp:
    if any(w == 4 for w, *_ in port[addr]):
        print("PRÜFEN 0x%04x: oberes Wort 0x%04x eigenständig in %s" % (addr, addr + 2, ", ".join(fns[:6])))
print("Abweichungen:", bad, " geprüfte Adressen:", len([a for a in port if a in orig]))

# --- indizierte Zugriffe: DS_xx(0xNNNN + ...) gegen [reg+0xNNNN] ---
pat2 = re.compile(r"DS_(I8|U8|I16|U16|I32|U32)\(0x([0-9a-fA-F]+) \+")
cnt = bad2 = 0
for f in glob.glob(os.path.join(ROOT, "port", "src", "game", "*.c")):
    for n, line in enumerate(open(f, encoding="utf-8"), 1):
        for m in pat2.finditer(line):
            a, w = int(m.group(2), 16), W[m.group(1)]
            o = orig_idx.get(a)
            if not o:
                continue
            cnt += 1
            eff = set(o)
            if 2 in o and 2 in orig_idx.get(a + 2, set()):
                eff.add(4)
            if w > max(eff) or w < min(eff):
                bad2 += 1
                print("IDX %s 0x%04x port=%d orig=%s  %s:%d  %s" % ("BREITER" if w > max(eff) else "schmaler", a, w,
                      sorted(o), os.path.basename(f), n, line.strip()[:100]))
print("indiziert geprüft:", cnt, "Abweichungen:", bad2)

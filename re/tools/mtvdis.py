"""Schneller 16-Bit-Disassembler für MTV.EXE (Capstone).

Aufruf: python mtvdis.py seg:off[:anzahl] ...   (anzahl = Instruktionen, Standard 40)
Bis zum ersten RETF wird gelesen, wenn anzahl 0 ist. Bekannte Funktionsadressen werden
über re/notes/names.tsv aufgelöst, Far-Calls über die MZ-Relokation (Ladesegment 0x1000).
Benötigt capstone (PYTHONPATH auf scratchpad/pylib).
"""
import os
import struct
import sys

import capstone

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = open(os.path.join(ROOT, "re", "v1", "MTV.EXE"), "rb").read()
HDR = struct.unpack_from("<H", EXE, 8)[0] * 16
NAMES = {}
names_path = os.path.join(ROOT, "re", "notes", "names.tsv")
if os.path.exists(names_path):
    for line in open(names_path, encoding="utf-8"):
        a, n = line.rstrip("\n").split("\t")[:2]
        NAMES[a] = n

# Relokationen: Stellen, an denen ein Segmentwort steht (+0x1000 beim Laden)
RELOCS = set()
n_rel, rel_off = struct.unpack_from("<H", EXE, 6)[0], struct.unpack_from("<H", EXE, 0x18)[0]
for i in range(n_rel):
    o, s = struct.unpack_from("<HH", EXE, rel_off + 4 * i)
    RELOCS.add(s * 16 + o)

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)


def dis(seg, off, count):
    lin = (seg - 0x1000) * 16 + off
    code = EXE[HDR + lin: HDR + lin + 4096]
    n = 0
    for ins in md.disasm(code, off):
        text = "%s %s" % (ins.mnemonic, ins.op_str)
        if ins.mnemonic == "lcall" and ins.size == 5:
            o, s = struct.unpack_from("<HH", code, ins.address - off + 1)
            tgt = "%04x:%04x" % (s + 0x1000, o)
            text += "    ; " + NAMES.get(tgt, tgt)
        elif ins.mnemonic == "call" and ins.size == 3:
            tgt = "%04x:%04x" % (seg, int(ins.op_str, 16) & 0xFFFF)
            text += "    ; " + NAMES.get(tgt, tgt)
        print("%04x:%04x  %-20s %s" % (seg, ins.address, ins.bytes.hex(), text))
        n += 1
        if (count and n >= count) or (not count and ins.mnemonic in ("retf", "ret") and n > 3):
            break


for arg in sys.argv[1:]:
    p = arg.split(":")
    dis(int(p[0], 16), int(p[1], 16), int(p[2]) if len(p) > 2 else 40)
    print()

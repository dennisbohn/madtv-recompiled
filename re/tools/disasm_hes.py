"""Disassemble a PALLADIX-PSM driver (.HES, raw 16-bit code, CS:0 = file start).
Usage: python disasm_hes.py <file.HES> [start_hex ...]
Linear sweep from 0x40 with slot-entry labels; data regions are shown as db lines
when start points are given via 'data' ranges (simple)."""
import sys, struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
d = open(sys.argv[1], 'rb').read()
n = struct.unpack_from('<H', d, 0x1e)[0]
slots = struct.unpack_from('<%dH' % n, d, 0x20)
lab = {o: 'slot%d' % i for i, o in enumerate(slots)}
md = Cs(CS_ARCH_X86, CS_MODE_16)
md.skipdata = True
start = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x40
end = int(sys.argv[3], 16) if len(sys.argv) > 3 else len(d)
for ins in md.disasm(d[start:end], start):
    if ins.address in lab:
        print('\n;----- %s -----' % lab[ins.address])
    print('%04x: %-20s %s %s' % (ins.address, ins.bytes.hex(), ins.mnemonic, ins.op_str))

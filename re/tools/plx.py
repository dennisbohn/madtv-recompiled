"""PLX-Musikdateien (PALLADIX-PSM AdLib, nach LZW) analysieren / validieren.

Aufruf: python -I plx.py <Gamefiles-Ordner> [HES-Nr ...] [-v]
Format siehe re/notes/sound_driver.md.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lzw import lzw_decode  # noqa: E402

PIT_HZ = 1193182


def parse_stream(u, ptr, perc_ch=False):
    """Liefert Liste (tick, cmd, operands) und Endstatus."""
    ev = []
    si = ptr
    t = 0
    while True:
        cmd = u[si]; si += 1
        if cmd == 0:
            ev.append((t, 'END', None))
            return ev, si
        ops = {}
        if cmd != 0x80:
            c = cmd
            if c & 1:
                ops['inst'] = struct.unpack_from('<H', u, si)[0]; si += 2
            if c & 2:
                ops['vol'] = u[si]; si += 1
            if c & 4:
                ops['keyoff'] = 1
            if c & 8:
                ops['note'] = u[si]; si += 1
            elif c & 0x10:
                ops['freq'] = struct.unpack_from('<H', u, si)[0]; si += 2
            if c & 0x20:
                ops['keyon'] = 1
            if c & 0x40:
                rest = c & 0xc7 & ~7
                ops['tempo'] = struct.unpack_from('<H', u, si)[0]; si += 2
            if c & 0x80:
                ev.append((t, 'BADCMD', cmd))
                return ev, si
        d = u[si]; si += 1
        ev.append((t, cmd, ops))
        t += max(d, 1) if False else d
        # Hinweis: der Treiber verarbeitet pro Kanal max. 1 Event/Tick,
        # Delta 0 wirkt also wie Delta 1 (siehe Doku).


def main():
    g = sys.argv[1]
    verbose = '-v' in sys.argv
    nums = [int(a) for a in sys.argv[2:] if a != '-v'] or \
        [78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94,
         96, 97, 147, 148, 149, 254, 255]
    for n in nums:
        d = open(os.path.join(g, '%03d.HES' % n), 'rb').read()
        u = lzw_decode(d)
        assert u[:3] == b'PLX', n
        typ, speed, tempo = u[3], u[4], struct.unpack_from('<H', u, 5)[0]
        nch = 11 if typ == 1 else 9
        ptrs = struct.unpack_from('<%dH' % nch, u, 7)
        tick_hz = PIT_HZ / (tempo * max(speed, 1))
        insts = set()
        notes = set()
        tempos = set()
        maxlen = 0
        zero_deltas = 0
        bad = []
        for ch, p in enumerate(ptrs):
            if not p:
                continue
            ev, end = parse_stream(u, p)
            for t, c, o in ev:
                if c in ('END', 'BADCMD'):
                    if c == 'BADCMD':
                        bad.append((ch, o))
                    maxlen = max(maxlen, t)
                    continue
                insts.update([o['inst']] if 'inst' in o else [])
                notes.update([o['note']] if 'note' in o else [])
                tempos.update([o['tempo']] if 'tempo' in o else [])
            if verbose:
                print('  ch%d @%04x..%04x' % (ch, p, end))
                for t, c, o in ev[:40]:
                    print('    t=%5d %s %s' % (t, c if isinstance(c, str) else '%02x' % c, o))
        print('%03d typ=%d speed=%d tempo=0x%04x tick=%.3f Hz chans=%s len=%d ticks=%.1fs insts=%s notes=%s..%s tempos=%s bad=%s' % (
            n, typ, speed, tempo, tick_hz, [i for i, p in enumerate(ptrs) if p], maxlen,
            maxlen / tick_hz, sorted('%x' % i for i in insts), min(notes or [0]), max(notes or [0]),
            sorted('%x' % x for x in tempos), bad))


if __name__ == '__main__':
    main()

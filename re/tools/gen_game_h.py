"""Erzeugt aus re/notes/sigs/*.tsv:
  port/src/game/game.h      Prototypen aller Spielfunktionen
  port/src/game/stubs.c     Platzhalter für noch nicht übersetzte Funktionen
  re/notes/names.tsv        Adresse -> Name (Spielfunktionen + Laufzeitbibliothek)

Eine Funktion gilt als übersetzt, wenn ihr Name in einer Datei port/src/game/seg_*.c
als Definition vorkommt ("<name>(" am Zeilenanfang nach dem Rückgabetyp).

Aufruf: python -I gen_game_h.py <Projektwurzel>
"""
import os
import re
import sys

# Bibliotheksfunktionen (Originaladresse -> Name in port/src/rtlib.h)
LIBRARY = {
    "43cb:0045": "vga_detect", "43cf:0005": "vga_detect", "43cf:0009": "vga_set_mode13",
    "43d1:0000": "vga_restore_mode", "43d2:0001": "vga_wait_vsync", "43d3:0001": "vga_set_palette",
    "43d5:0008": "vga_init_segs", "43d7:0006": "scr_grab", "43de:0009": "scr_blit",
    "43e5:000d": "bmp_fill", "43eb:0009": "bmp_blit", "43f2:0008": "bmp_blit_rect",
    "43fd:0003": "bmp_blit_rect_shift", "440a:000c": "bmp_glyph_trans",
    "4416:0005": "bmp_blit_rect_trans", "4425:000c": "bmp_blit_trans_shift",
    "4432:0001": "bmp_blit_rect_trans_shift", "44e4:000d": "scr_blit_rect",
    "44e4:00a8": "kbd_init", "44ee:0008": "kbd_init", "44bc:000e": "bitmap_alloc",
    "44c3:0001": "bitmap_alloc", "44c9:0004": "spritebank_size",
    "43c6:0007": "mouse_init", "43c7:0005": "mouse_setup", "43cb:0009": "mouse_poll",
    "44ee:000c": "kbd_flush", "44ef:000a": "kbd_getkey", "44ef:0009": "kbd_shutdown",
    "44ce:012c": "hes_filesize", "44ce:0007": "hes_sprite_info", "44f6:0001": "hes_list",
    "4500:0006": "hes_load", "4510:0003": "hes_read", "4505:000a": "hes_write",
    "450b:0005": "hes_write_dgroup", "44f3:0009": "dos_get_drive", "44ff:0004": "dos_set_drive",
    "438d:0002": "lzw_decode", "4444:04ac": "hes_unpack_sprites", "5052:0000": "rle_delta_frame_bmp",
    "42fe:000a": "timer_init", "42a5:059a": "timer_init", "42fe:000e": "ticks_reset",
    "42fe:0015": "timer_shutdown", "42fe:0016": "ticks_get", "42fe:001a": "ticks_set",
    "4519:0010": "tc_exit", "451d:0003": "tc_atexit", "4520:0002": "tc_malloc",
    "4520:0203": "tc_farmalloc", "4568:0088": "tc_itoa", "4568:00b5": "tc_ultoa",
    "4568:00d5": "tc_ltoa", "4578:0000": "strlen", "4579:000b": "strcpy", "457c:0004": "setmem",
    "457c:002a": "memset", "4580:0009": "tc_farcalloc", "4589:000f": "tc_free",
    "4589:02e9": "tc_free", "45bc:0003": "abs", "45bd:007a": "tc_atoi", "45c5:000d": "tc_calloc",
    "45cc:0009": "tc_srand", "45cc:001a": "tc_rand", "45d0:0002": "strncpy",
    "1000:0412": "LXMUL@ (Operator *)", "1000:042b": "LDIV@ (Operator /)",
    "1000:042f": "LUDIV@ (Operator /, unsigned)", "1000:0434": "LMOD@ (Operator %)",
    "1000:0439": "LUMOD@ (Operator %, unsigned)", "1000:04ce": "LXLSH@ (Operator <<)",
    "1000:050a": "LXLSH@ (Operator <<)", "1000:0529": "SCOPY@ (Struct-Zuweisung)",
    "1000:0313": "PADA@ (Zeiger += long)", "1000:0396": "PADD@ (Zeiger + long)",
    "1000:03f1": "PCMP@ (Zeigervergleich)",
}
# Soundtreiber-Slots
for k in range(13):
    LIBRARY["43ae:%04x" % (0x83 + 5 * k)] = "psm_slot_%d" % k

DEF_RE = re.compile(r"^[A-Za-z_][\w \*]*?\b(\w+)\s*\([^;]*$")


def main(root):
    sig_dir = os.path.join(root, "re", "notes", "sigs")
    game_dir = os.path.join(root, "port", "src", "game")
    os.makedirs(game_dir, exist_ok=True)

    rows = []
    for name in sorted(os.listdir(sig_dir)):
        if not name.endswith(".tsv"):
            continue
        for line in open(os.path.join(sig_dir, name), encoding="utf-8"):
            parts = line.rstrip("\n").split("\t")
            if len(parts) < 3 or not re.match(r"^[0-9a-f]{4}:[0-9a-f]{4}$", parts[0]):
                continue
            addr, fname, sig = parts[0], parts[1], parts[2].strip().rstrip(";")
            if fname == "main":  # Original-main heißt im Port mtv_main (C-main ist der Starter)
                fname = "mtv_main"
                sig = re.sub(r"\bmain\s*\(", "mtv_main(", sig, count=1)
            if addr in LIBRARY:
                continue
            if len(parts) > 4 and any(k in parts[4].lower() for k in ("not a function", "do not port", "not a real function")):
                continue  # Codefragment innerhalb einer anderen Funktion
            rows.append((addr, fname, sig, parts[3] if len(parts) > 3 else ""))

    # bereits übersetzte Funktionen finden
    defined = set()
    for name in os.listdir(game_dir):
        if name.startswith("seg_") and name.endswith(".c"):
            for line in open(os.path.join(game_dir, name), encoding="utf-8"):
                m = DEF_RE.match(line)
                if m and not line.startswith(("static", "typedef", "return", "if", "for", "while", "switch")):
                    defined.add(m.group(1))

    names = {}
    for addr, fname, _, _ in rows:
        if fname in names.values():
            raise SystemExit("Name doppelt vergeben: %s (%s)" % (fname, addr))
        names[addr] = fname

    with open(os.path.join(game_dir, "game.h"), "w", encoding="utf-8", newline="\n") as f:
        f.write("/* GENERIERT von re/tools/gen_game_h.py aus re/notes/sigs - nicht von Hand ändern. */\n")
        f.write("#ifndef MADTV_GAME_H\n#define MADTV_GAME_H\n\n#include <stdint.h>\n\n")
        f.write('#include "dgroup.h"\n#include "rtlib.h"\n#include "sound.h"\n#include "globals.h"\n\n')
        seg = None
        for addr, fname, sig, conf in sorted(rows):
            if addr[:4] != seg:
                seg = addr[:4]
                f.write("\n/* ---- Segment %s ---- */\n" % seg)
            f.write("%s; /* %s%s */\n" % (sig, addr, "" if conf in ("high", "") else ", " + conf))
        f.write("\n#endif\n")

    with open(os.path.join(game_dir, "stubs.c"), "w", encoding="utf-8", newline="\n") as f:
        f.write("/* GENERIERT von re/tools/gen_game_h.py: Platzhalter für noch nicht übersetzte Funktionen. */\n")
        f.write('#include "game.h"\n#include "stub.h"\n\n')
        missing = 0
        for addr, fname, sig, _ in sorted(rows):
            if fname in defined:
                continue
            missing += 1
            ret = sig.split("(")[0].rsplit(fname, 1)[0].strip()
            body = 'STUB("%s", "%s");' % (fname, addr)
            if ret != "void":
                body += " return (%s)0;" % ret
            f.write("%s\n{\n    %s\n}\n\n" % (sig, body))

    with open(os.path.join(root, "re", "notes", "names.tsv"), "w", encoding="utf-8", newline="\n") as f:
        for addr, name in sorted(list(names.items()) + list(LIBRARY.items())):
            f.write("%s\t%s\n" % (addr, name))

    print("Funktionen: %d, übersetzt: %d, Platzhalter: %d" % (len(rows), len(rows) - missing, missing))


if __name__ == "__main__":
    main(sys.argv[1])

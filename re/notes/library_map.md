# MTV.EXE – Library-Map (Segment 1000 und 42a0 … 45d0)

Quelle: `re/out/decompiled.c` (Zeilen 1–1080 und 87856–91575), `functions.tsv`, `interrupts.tsv`,
eigene Disassemblierung von `Gamefiles/MTV.EXE` (Capstone, 16 Bit) und Auswertung der
MZ-Relokationstabelle.

**Aufrufzählung:** "Aufrufe" = Anzahl Call-Sites im ganzen EXE. `far` wurde exakt über die
Relokationstabelle ermittelt (jeder `CALL FAR seg:off` hat einen Reloc-Eintrag), `near` durch
lineares Disassemblieren der Library-Segmente (`push cs; call near` innerhalb desselben Moduls).
Aliase (gleiche lineare Adresse, anderes seg:off, z. B. `43cf:0005` = `43cb:0045`) sind
zusammengezählt. Die Spalte `callers` in `functions.tsv` zählt dagegen *verschiedene aufrufende
Funktionen* und ist daher kleiner.

**Wichtigste Erkenntnis vorab:** Der Bereich 42a0–45d0 ist *nicht nur* Laufzeitbibliothek.
Er besteht aus vier Schichten (Reihenfolge = Link-Reihenfolge):

| Bereich | Inhalt | Herkunft |
|---|---|---|
| 42a0–4301 | Spiel-Module in C (Bitflags, Musik-Steuerung, Tick-Zähler, Intro-Sequenz) | Spielcode (C) |
| 438d, 4444 | Dekompressoren (LZW, Order-1-Huffman für Sprite-Bänke) | Spielcode (C) |
| 43ae | Lader/Stub-Tabelle für den **PALLADIX-PSM**-Soundtreiber | Assembler |
| 43c6–4510 | Eigene Assembler-Library: Maus, VGA, Palette, VSync, Blitter, Tastatur, `NNN.HES`-Datei-I/O | Assembler |
| 4516–45d0 | Turbo-C-2.0-Laufzeitbibliothek (exit/atexit, Far-Heap, Strings, rand, itoa …) | Borland RTL |
| 1000 | Turbo-C-Startup `c0l` + Compiler-Helfer (`LXMUL@`, `LDIV@`, `PADD@`, `SCOPY@` …) | Borland RTL |

Es wird **kein** `stdio` (`printf`, `FILE*`, `fopen`) und kein Turbo-C-Grafik-BGI benutzt.
Alle Daten liegen in nummerierten Dateien `NNN.HES` (001.HES … 099.HES) im aktuellen
Verzeichnis und werden nur über die eigenen Asm-Routinen geladen.

---

## 0. Gemeinsame Datenstrukturen

### Bitmap (von allen Blittern benutzt)

```c
typedef struct {            /* 8 Byte Header, Pixel folgen direkt dahinter (bitmap_alloc) */
    uint16_t wcols;         /* Breite in 8-Pixel-Spalten (Breite_px = wcols*8)           */
    uint16_t h;             /* Höhe in Zeilen                                           */
    uint8_t  far *pixels;   /* Offset, Segment; zeilenweise, stride = wcols*8, 1 Byte/Px */
} Bitmap;
```

* Alle x-Koordinaten der Blitter sind in **8-Pixel-Spalten**, außer bei den `*_shift`-Varianten,
  die zusätzlich einen Pixel-Offset 0..7 nehmen.
* Breitenparameter werden mit `& 0x1fff` maskiert (obere Bits evtl. Flags, ungenutzt).
* Transparente Varianten: Farbindex **0 = transparent**.
* Bildschirm = Mode 13h, 320×200, Segment in `DS:16a6`/`DS:16a8` (= `A000h`).

### Sprite-Bank (Ergebnis von `hes_unpack_sprites` 4444:04ac)

```c
struct SpriteBank { uint16_t count; Bitmap bmp[count]; uint8_t pixels[]; };
```

Rohformat in der .HES-Datei: `u8 N; N×{u8 w_px, u8 h}; u16be ?; u32be out_len; Bitstrom`.

### Spiel-Tastencodes (`kbd_getkey` 44ef:000a, Tabellen `DS:16ac` ASCII, `DS:172e` Scancode)

F1–F10 → 0x00–0x09, `0`–`9` → 0x0a–0x13, ESC → 0x14, ↑ 0x15, ↓ 0x16, ← 0x17, → 0x18,
`-` 0x1d, `+` 0x1e, Enter 0x1f, Space 0x20, Backspace 0x21, `,` 0x22, `.` 0x23,
`A`–`Z`/`a`–`z` → 0x26–0x3f, sonst –1 (kein Umlaut-Support).

---

## 1. Segment 1000 – Turbo-C-Startup und Compiler-Helfer

Register-Aufrufkonvention der Helfer: 32-Bit-Werte in `DX:AX` bzw. `CX:BX`; Ghidra zeigt das
als `in_AX`, `in_CX` usw. – die dekompilierten Rümpfe von 0396/03f1/0412 sind dadurch
irreführend (z. B. `0412` erscheint als `in_AX*in_BX`, ist aber die volle 32×32-Multiplikation).

| Adresse | Name (TC 2.0) | Signatur | Zweck | Aufrufe | Port-Strategie |
|---|---|---|---|---|---|
| 1000:0000 | `_c0_start` | – | DOS-Version, PSP/Env-Größe, `87=`-Env-Variable (`_8087`), Heap-Top, `INT21/4A` Speicher verkleinern, BSS nullen, `SaveVectors`, `_setargv`, `_setenvp`, BIOS-Uhr (`INT1A`) nach `DS:83/85`, `main(argc,argv,envp)` = **1054:0005**, dann `exit()` | Entry | entfällt – normales `int main()` |
| 1000:010d | `_exit` / `_terminate` | `void far _exit(int code)` | Vektoren zurück, Hook `[1946]`, `INT21/4C` | 2 (exit, abort) | `exit()` |
| 1000:0125 | (`Int0Handler`) | ISR | "Divide error" ausgeben, Exit 3 | Vektor | entfällt |
| 1000:012f | `SaveVectors` | near | INT 0/4/5/6 sichern, INT 0 umbiegen | 1 | entfällt |
| 1000:0172 | `_restorezero` | far | INT 0/4/5/6 restaurieren | 1 | entfällt |
| 1000:019f | (`_8087`-Reset) | far | `DS:0081 = 0` | 0 (Funktionszeiger) | entfällt |
| 1000:01a6 | `_dummy` | far | leeres `retf` (FP-Hook-Platzhalter `[1944]/[1946]/[1948]`) | 0 | entfällt |
| 1000:01a7 | `_ErrorMessage` | near | Text nach stderr (`INT21/40`, BX=2) | 1 | `fputs(stderr)` |
| 1000:01af | `abort` | far | "Abnormal program termination", Exit 3 | 0 direkt | `abort()` |
| 1000:01d1 | `_setargv` | far | Kommandozeile → `argv[]` | 1 | entfällt (SDL_main) |
| 1000:025f | `_setargv`-Helfer | near | Zeichen-Klassifikation (Quotes, Whitespace) | 2 | entfällt |
| 1000:02cf | `_setenvp` | far | `envp[]` per `malloc` aufbauen | 1 | entfällt |
| 1000:0313 | `PADA@` | `huge* += long` (in-place, ES:BX) | Huge-Pointer + long, normalisiert | 1 | entfällt – flacher 32-Bit-Zeiger |
| 1000:0355 | `PSBA@` | `huge* -= long` | (keine Ghidra-Funktion) | 0 | entfällt |
| 1000:0396 | `PADD@` | `DX:AX = huge + CX:BX` | Huge-Pointer-Addition, Ergebnis normalisiert | 6 (nur Far-Heap) | entfällt |
| 1000:03c3 | `PSUB@` | `huge - long` | (keine Ghidra-Funktion) | 0 | entfällt |
| 1000:03f1 | `PCMP@` | Flags = cmp(DX:AX, CX:BX) | Huge-Pointer-Vergleich | 14 (nur Far-Heap) | entfällt |
| 1000:0412 | **`LXMUL@`** | `DX:AX = DX:AX * CX:BX` | 32-Bit-Multiplikation | **79** | entfällt – native `int32_t`-Arithmetik |
| 1000:042b | **`LDIV@`** | `long LDIV@(long a, long b)` (Pascal, `retf 8`) | signierte 32-Bit-Division | **67** | entfällt – `/` |
| 1000:042f | `LUDIV@` | unsigned | 32-Bit-Division unsigned | 2 | entfällt |
| 1000:0434 | `LMOD@` | signed | 32-Bit-Modulo (keine Ghidra-Funktion) | 0 | entfällt |
| 1000:0439 | `LUMOD@` | unsigned | 32-Bit-Modulo unsigned | 0 | entfällt |
| 1000:043c | (Kern Division) | CX = Modus-Bits 0..3 | gemeinsamer Divisionskern | – | entfällt |
| 1000:04ce | `LXLSH@` | `DX:AX <<= CL` | 32-Bit-Shift links (Schleife) | 6 (Huffman) | entfällt |
| 1000:04e3 | `LXRSH@` | `DX:AX >>= CL` signed | (keine Ghidra-Funktion) | 0 | entfällt |
| 1000:04f5 | `LXURSH@` | unsigned | (keine Ghidra-Funktion) | 0 | entfällt |
| 1000:050a | `LXLSH@` (schnelle Variante) | `DX:AX <<= CL` | 32-Bit-Shift ohne Schleife | 1 (seg 4151) | entfällt |
| 1000:0529 | `SCOPY@` | `SCOPY@(far *src, far *dst)`, CX = Größe | Struct-Zuweisung (`rep movsw`) | 7 | entfällt – `struct a = b` / `memcpy` |

Hinweis: Fast jede Funktion, die `LXMUL@`/`LDIV@` aufruft, rechnet mit `long` (Geld, Zuschauer).
Beim Portieren auf `int32_t` achten (nicht `long` unter LP64, nicht `int`).

---

## 2. Segmente 42a0–4301 – Spiel-Module (C, im Library-Bereich gelinkt)

| Adresse | Vorgeschlagener Name | Signatur | Zweck | Aufrufe | Port-Strategie |
|---|---|---|---|---|---|
| 42a0:0001 | `bit_set` | `void bit_set(uint16_t far *w, int bit)` | `*w |= 1<<bit` | 2 | 1:1 übernehmen / Makro |
| 42a0:0016 | `bit_clear` | `void bit_clear(uint16_t far *w, int bit)` | `*w &= ~(1<<bit)` | 1 | Makro |
| 42a0:002d | `bit_test` | `int bit_test(uint16_t w, int bit)` | Bit prüfen | 5 | Makro |
| 42a5:0002 | `snd_init` | `void snd_init(void)` | lädt `099.HES` (PSMDETECT, 0x15c B), ruft Detect, sucht Treiber-Eintrag Typ 1 (AdLib) sonst Typ 0 (Beeper), setzt `DS:1602` (1 = AdLib), lädt `098.HES` (AdLib, 0x1574 B) bzw. `095.HES` (PC-Beeper, 0xe1e B), installiert ihn mit 4 Parametern aus dem 0x50-Byte-Detect-Record, ruft Treiber-Slot 5(0) | 1 (main) | ersetzen: eigener Audio-Init (SDL_audio + OPL-Emulator) |
| 42a5:01a8 | `music_load_play` | `void music_load_play(int hes_no)` | `NNN.HES` laden, **LZW** (438d) entpacken, Treiber-Slot 6(ptr,len) | 1 | ersetzen (PSM-Song-Player) |
| 42a5:027f | `music_free` | `void music_free(void)` | Song-Puffer freigeben (`1c74:06c9`) | 1 | ersetzen |
| 42a5:028f | `music_start` | `void music_start(void)` | Slot 5(1), Slot 9(4,0) | 1 | ersetzen |
| 42a5:02a8 | `music_stop` | `void music_stop(void)` | Slot 9(2,0), Slot 5(0) | 1 | ersetzen |
| 42a5:02c0 | `snd_shutdown` | `void snd_shutdown(void)` | Treiber deinstallieren (43ae:0164); per `atexit` registriert | atexit | `SDL_CloseAudio` |
| 42a5:02c6 | `music_on_enter` | `void music_on_enter(int room, int player)` | Raum-ID → Song-Nr. (0x4e–0x95, z. B. Raum 1→0x61) – **nur bei AdLib** (`1602==1`) | 6 | Logik 1:1 übernehmen, Player ersetzen |
| 42a5:04dd | `music_on_leave` | `void music_on_leave(int room, int player)` | Song stoppen/freigeben bei Raumwechsel | 8 | Logik übernehmen |
| 42a5:0582 | `para_align` | `void far *para_align(void far *p)` | `(seg+1):(off & 0xfff0)` – paragraph-ausgerichteter Zeiger für Treiber | 4 | entfällt |
| 42a5:059a | `timer_init` (Stub) | `int f(void)` → –1 | Alias 42fe:000a, aus main gerufen | 1 | entfällt |
| 42fe:000e | `ticks_reset` | `void ticks_reset(void)` | `DS:9014 = 0` | 2 | ersetzen |
| 42fe:0015 | `timer_shutdown` (leer) | – | per `atexit` registriert | 1+atexit | entfällt |
| 42fe:0016 | **`ticks_get`** | `uint16_t ticks_get(void)` | liest Tick-Zähler `DS:9014` | **26** | **ersetzen: `SDL_GetTicks()`-basierter Zähler** (Rate s. offene Fragen) |
| 42fe:001a | `ticks_set` | `void ticks_set(uint16_t n)` | Zähler setzen + Treiber-Slot 3(`&DS:900c`) – registriert Zählerstruktur beim Treiber-Timer-IRQ | 8 | ersetzen |
| 4301:0001 | `intro_load_frame` | `void far *intro_load_frame(int res)` | Ressource laden + LZW-Entpacken | 0 (intern) | Spielcode portieren |
| 4301:0090 | `intro_scene_setup` | `void (void)` | Szene `DS:9032` vorbereiten (Hintergrund + 3 Animationsstreams) | 17 near | Spielcode |
| 4301:01f7 | `intro_frame_step` | `void (void)` | VSync + **RLE-Delta-Frame** (5052:0000) in Puffer | 1 | Spielcode |
| 4301:0212 | `intro_tick` | `void (void)` | Animationszustand, Blit nach Screen (43de) | 1 | Spielcode |
| 4301:0361 | `intro_run_scene` | `void (void)` | Szene abspielen bis Taste/Maus oder Ende; Timing über `ticks_get` | 16 near | Spielcode |
| 4301:03ad | `intro_play` | `void intro_play(void)` | gesamte Intro-/Titelsequenz (14 Szenen, Abbruch per Taste/Maus) | 1 (main) | Spielcode portieren |

---

## 3. Dekompressoren

| Adresse | Name | Signatur | Zweck | Aufrufe | Port-Strategie |
|---|---|---|---|---|---|
| 438d:0002 | **`lzw_decode`** | `void lzw_decode(uint8_t far *dst, uint8_t far *src, void far *work8k)` | LZW: Quelle `+2` = komprimierte Länge (u16), Daten ab `+8`; Startbreite 9 Bit, Code **0x100 = Reset**, **0x101 = Codebreite+1**; Wörterbuch 4 Byte/Eintrag {prefix, first, last}, Arbeitspuffer 0x2000 Byte; Ende wenn Eingabe aufgebraucht | 2 (Musik, Intro) | in portablem C neu schreiben (Logik 1:1) |
| 438d:00d8 | `lzw_init_dict` | near | 256 Grundeinträge | 1 | s. o. |
| 438d:0119 | `lzw_read_code` | near | nächsten Code lesen (MSB-first, Masken `DS:1690`) | 2 | s. o. |
| 438d:01b7 | `lzw_emit` | near | String über Stack `DS:904b` rückwärts ausgeben | 1 | s. o. |
| 4444:0002 | `huf_getbyte` | near | nächstes Eingabebyte | (intern) | s. u. |
| 4444:0019 | `huf_putpixel` | near | Byte in aktuelles Sprite schreiben, legt Bitmap-Header an (Breite auf 8 aufgerundet) | 5 near | s. u. |
| 4444:013d | `huf_getbit` | near | nächstes Bit, **LSB-first** | 7 near | s. u. |
| 4444:0166 | `huf_read_tree` | rekursiv | Baum in Preorder: Bit 0 = innerer Knoten (IDs ab 0x101), Bit 1 = Blatt; das erste Blatt jedes Baums ist der **Escape (0x100)**, übrige Blätter = 8-Bit-Literal | 4 near | s. u. |
| 4444:01fe | `huf_decode` | near | **Order-1-Kontext-Huffman**: 1 Startbaum + 256 Bäume (nach vorherigem Byte); Escape → 8 rohe Bits | 2 near | s. u. |
| 4444:04ac | **`hes_unpack_sprites`** | `void hes_unpack_sprites(uint8_t far *src, SpriteBank far *dst)` | Sprite-Bank entpacken (Header s. Abschnitt 0) | 3 (seg 1c74) | **neu in C schreiben**, Assets am besten einmalig in PNG/Rohformat konvertieren |
| 5052:0000 | `rle_delta_frame` | `void rle_delta_frame(uint8_t far **stream, Bitmap far *dst)` | Animations-Delta (Intro): `0x62`=Frame-Ende, `0x63` n v = Run (n summiert über 0xff-Bytes; v=`0x64` → n Pixel überspringen), `0x64` = 1 Pixel überspringen, sonst Literal. **Liegt hinter DGROUP** (Segment 5052, Ghidra als Daten markiert) | 1 (4301) | in C neu schreiben |

---

## 4. Segment 43ae – PALLADIX-PSM-Treiberschnittstelle (Assembler)

Das Segment beginnt mit den Signaturen `"PALLADIX-PSMDETECT\r\n$"` (0x24 Byte) und
`"PALLADIX-PSM\r\n\x1a$"` (0x20 Byte). Ab `43ae:0083` liegen **13 Far-Jump-Slots** à 5 Byte
(`jmp far 43ae:00c4`, Default-Stub `mov ax,-1; cwd; retf`). `snd_install` vergleicht den Header
des geladenen Treibers (`095.HES` "V1.1 – PC-Beeper", `098.HES` "V1.3 – AdLib (TM) soundcard")
und trägt dessen 13 Einsprungoffsets mit dem Treibersegment in die Slots ein.

**Achtung Ghidra:** Alle Slots heißen dort `thunk_FUN_43ae_00c4` – unterscheidbar nur über
die Adresse.

| Adresse | Name | Signatur | Zweck | Aufrufe | Port |
|---|---|---|---|---|---|
| 43ae:0024 | `psm_detect_entry` | `void far *psm_detect_entry(void far *buf)` | prüft Detect-Signatur, liefert Einsprung des Detect-Moduls (Rückgabe: Tabelle mit 0x50-Byte-Records: `int id(-1=Ende), int type(1=AdLib,0=Beeper), char file[], char name[], … +0x48..0x4e: 4 Init-Parameter`) | 1 | ersetzen (Audio-Device-Auswahl) |
| 43ae:0083+5k | `psm_slot[k]` | far | Treiberfunktionen. Beobachtet: k=0 Init(4 Params), k=1 Shutdown, k=3 Zählerstruktur registrieren(far ptr) [Vermutung], k=5 Musik an/aus(int), k=6 Song setzen(far ptr, len), k=9 Kommando(cmd,arg) | k3:1, k5:3, k6:1, k9:2 | ersetzen |
| 43ae:00c4 | `psm_stub` | `long f(void)` → -1 | Default für nicht installierte Slots | 13 jmp | entfällt |
| 43ae:00c9 | `psm_reset_slots` | near | Slots auf Stub zurücksetzen | 3 near | entfällt |
| 43ae:00e5 | `psm_install` | `int psm_install(void far *drv, int p1,int p2,int p3,int p4)` | Treiber-Header prüfen, Slots patchen, Slot 0 (Init) rufen; Fehler → -2 | 1 | ersetzen |
| 43ae:0164 | `psm_uninstall` | `void psm_uninstall(void)` | Slot 1 (Shutdown), Slots zurücksetzen | 2 | ersetzen |

Port-Strategie Sound: Musikformat der Song-Dateien (LZW-entpackt) und das Treiberprotokoll
gesondert analysieren; danach OPL2-Emulator (z. B. Nuked OPL3 / DOSBox-OPL) hinter
SDL-Audio. Für einen ersten lauffähigen Port genügt ein Stub (keine Musik); der Tick-Zähler
muss aber unabhängig vom Treiber laufen (s. offene Fragen).

---

## 5. Eigene Assembler-Hardware-Library (43c6–44ef)

Erkennbar an `mov ax,35d4h; mov ds,ax` (unrelozierter DGROUP-Wert) und Register-Sicherung
ohne C-Prolog.

### Maus (INT 33h)

| Adresse | Name | Signatur | Zweck | Aufrufe | Port-Strategie |
|---|---|---|---|---|---|
| 43c6:0007 | `mouse_init` | `int mouse_init(void)` | `INT33/0`; 1 = Maus vorhanden, 0 = nein | 1 | `return 1;` |
| 43c7:0005 | `mouse_setup` | `void mouse_setup(void)` | Bereich X/Y 0..3200 (AX=7/8), Mickey-Ratio 3/3 (AX=0Fh), Position 1600/1600 (AX=4), Delta `DS:16a2/16a4`=0 | 1 | `SDL_SetRelativeMouseMode(SDL_TRUE)` |
| 43cb:0009 | **`mouse_poll`** | `int mouse_poll(void)` → Buttons (BX) | `INT33/3`, Delta = `pos/8 - 200` nach `DS:16a2` (dx) / `DS:16a4` (dy), Maus wieder auf 1600/1600 zentrieren. **Relative Maus**; Ghidra zeigt falschen Rückgabewert (`0x45d4`) | 3 | `SDL_GetRelativeMouseState` (Skalierung: Mickeys/8) + Buttons |

### Video / Palette / VSync

| Adresse | Name | Signatur | Zweck | Aufrufe | Port-Strategie |
|---|---|---|---|---|---|
| 43cb:0045 (= 43cf:0005) | `vga_detect` | `int vga_detect(void)` → 0x100 | Stub: immer 256 Farben | 1 | `return 0x100;` |
| 43cf:0009 | `vga_set_mode13` | `void vga_set_mode13(void)` | alten Modus (`INT10/0F`) nach `DS:16aa`, `INT10 AX=13h` | 1 | SDL-Fenster + 320×200 8-Bit-Surface/Texture anlegen |
| 43d1:0000 (= 43cf:0020) | `vga_restore_mode` | `void vga_restore_mode(void)` | alten Modus zurück; **nur per `atexit`** registriert (keine Ghidra-Funktion) | atexit | SDL-Fenster schließen |
| 43d2:0001 | **`vga_wait_vsync`** | `void vga_wait_vsync(void)` | Port 3DAh Bit 3: Ende abwarten, dann Beginn | 2 | Present mit VSync / Frame-Limiter ~70 Hz |
| 43d3:0001 | **`vga_set_palette`** | `void vga_set_palette(uint16_t far *pal)` | `pal[0]` = Anzahl, danach RGB-Tripel (6 Bit), `INT10 AX=1012h`, Start 0 | 2 | Palette in SDL-Palette übernehmen (Wert*255/63) |
| 43d5:0008 | `vga_init_segs` | `void vga_init_segs(int,int)` | `DS:16a6 = DS:16a8 = A000h` (Ziel-/Quellsegment Bildschirm) | 1 | Zeiger auf eigenen 64000-Byte-Framebuffer |

### Blitter (Mode 13h, 8-Pixel-Spalten)

Parameter in Klammern `{…}` = Funktion bekommt **einen Far-Zeiger auf einen Parameterblock**
(Ghidra zeigt `int *param_1`); Reihenfolge der Wörter wie angegeben.

| Adresse | Name | Signatur | Zweck | Aufrufe | Port-Strategie |
|---|---|---|---|---|---|
| 43d7:0006 | `scr_grab` | `void scr_grab(Bitmap far *dst, int xcol, int y)` | Bildschirmrechteck in Bitmap kopieren | **16** | `memcpy` aus SDL-Framebuffer |
| 43de:0009 | **`scr_blit`** | `void scr_blit(Bitmap far *src, int xcol, int y)` | Bitmap opak auf Bildschirm (`rep movsw`, stride 320) | **37** | `memcpy` zeilenweise in Framebuffer |
| 43e5:000d | **`bmp_fill`** | `void bmp_fill(Bitmap far *dst, int xcol, int y, int wcols, int h, uint8_t color)` | Rechteck in Bitmap füllen | **17** | `memset` zeilenweise |
| 43eb:0009 | `bmp_blit` | `void bmp_blit({Bitmap far *src; Bitmap far *dst; int xcol; int y})` | ganze Bitmap opak in Bitmap | 2 | C-Schleife |
| 43f2:0008 | `bmp_blit_rect` | `{src; int sx,sy,w,h; dst; int dx,dy}` (Spalten) | Teilrechteck opak Bitmap→Bitmap | 3 | C-Schleife |
| 43fd:0003 | `bmp_blit_rect_shift` | `{src; int xpix; int sx,sy,w,h; dst; int dx,dy}` | wie oben, mit Pixelversatz 0..7 | 1 | C-Schleife |
| 440a:000c | `bmp_glyph_trans` | `{src; int xpix; dst; int xcol; int y}` | **genau 8 Pixel breite** Bitmap transparent (0) mit Pixelversatz – vermutlich **Zeichen-/Font-Ausgabe** | 2 (seg 1f51) | C-Schleife mit Farbschlüssel 0 |
| 4416:0005 | `bmp_blit_rect_trans` | `{src; sx,sy,w,h; dst; dx,dy}` | Teilrechteck transparent | 1 | C |
| 4425:000c | `bmp_blit_trans_shift` | `{src; int xpix; dst; int xcol; int y}` | ganze Bitmap transparent mit Pixelversatz (Sprites) | 1 | C |
| 4432:0001 | `bmp_blit_rect_trans_shift` | `{src; xpix; sx,sy,w,h; dst; dx,dy}` | Teilrechteck transparent mit Pixelversatz | 1 | C |
| 44e4:000d | `scr_blit_rect` | `{src; int sx,sy,w,h; int dx,dy}` | Teilrechteck Bitmap→Bildschirm (fest `A000h`) | 2 | C |
| 44e4:00a8 (= 44ee:0008) | `kbd_init` (Stub) | `int f(void)` → 1 | aus main gerufen | 1 | entfällt |
| 44bc:000e | **`bitmap_alloc`** | `Bitmap far *bitmap_alloc(int w_px, int h)` | `malloc(ceil(w/8)*h*8 + 8)`, Header füllen | **65** | `malloc` + Struct; später evtl. eigener Typ |
| 44c3:0001 | `bitmap_alloc2` | identisch zu 44bc | Duplikat (anderes Objektmodul) | 1 | wie oben |
| 44c9:0004 | `spritebank_size` | `int spritebank_size(SpriteBank far *b)` | Gesamtgröße N*8+2+Σ Pixel | 3 | C |

Grafik-Hauptnutzer: Segment **1f51** (UI/Grafikschicht, 35 Aufrufe der Blitter), 1054 (main),
340d, 38b6, 1dd5, 4301 (Intro).

### Tastatur (INT 16h)

| Adresse | Name | Signatur | Zweck | Aufrufe | Port-Strategie |
|---|---|---|---|---|---|
| 44ee:000c | `kbd_flush` | `void kbd_flush(void)` | Tastaturpuffer leeren | 1 | SDL-Eventqueue leeren |
| 44ef:000a | **`kbd_getkey`** | `int kbd_getkey(void)` | nicht blockierend; ASCII → Tabelle `DS:16ac`, Erweitert (AL=0) → Scancode-Tabelle `DS:172e`; Spiel-Tastencode oder -1 | **8** | `SDL_PollEvent(SDL_KEYDOWN)` → gleiche Codes (Tabellen aus DGROUP übernehmen) |
| 44ef:0009 | `kbd_shutdown` (leer) | – | per `atexit` registriert | atexit | entfällt |

---

## 6. `NNN.HES`-Dateisystem und DOS-Helfer (44ce–4510, Assembler)

Dateiname wird global in `DS:17b0 "*.HES"` / `DS:17b6 "000.HES"` gebaut; alle Routinen nehmen
eine **Dateinummer** (0..999), keine Pfade. Rückgabe `long` (DX:AX), Fehler = -1.

| Adresse | Name | Signatur | Zweck | Aufrufe | Port-Strategie |
|---|---|---|---|---|---|
| 44ce:0105 | `hes_make_name` | `AX` = Nummer (Register!) | 3 Ziffern nach `DS:17b6` schreiben | 5 | `snprintf("%03d.HES")` |
| 44ce:012c | **`hes_filesize`** | `long hes_filesize(int no)` | Dateigröße per `findfirst` (`INT21/4E`, DTA) | **10** | `stat()` / `fseek+ftell` |
| 44ce:0007 | `hes_sprite_info` | `void hes_sprite_info(int no, long far *filesize, int far *unpacked_size)` | Header einer Sprite-Bank lesen (N, N×(w,h)), entpackte Größe berechnen | 1 | C mit `fread` |
| 44f6:0001 | `hes_list` | `int hes_list(int far *out)` | alle `*.HES` per `findfirst/findnext` aufzählen, `out[0]` = Anzahl, danach Nummern | 1 | Verzeichnis lesen (`FindFirstFile`/`opendir`) |
| 4500:0006 | **`hes_load`** | `long hes_load(int no, void far *buf)` | ganze Datei laden (`INT21/3D,3F` mit 0xFFFF Bytes, `3E`), max. 64 KB | **15** | `fopen/fread/fclose` |
| 4510:0003 | **`hes_read`** | `long hes_read(int no, void far *buf, uint16_t ofs, uint16_t len)` | Ausschnitt lesen (`INT21/42` Seek, nur 16-Bit-Offset) | **12** | `fopen/fseek/fread` |
| 4505:000a | `hes_write` | `long hes_write(int no, void far *buf, uint16_t len)` | Datei anlegen + schreiben (`3C,40,3E`) – Savegame | 2 | `fopen("wb")/fwrite` |
| 450b:0005 | `hes_write_dgroup` | `long hes_write_dgroup(int no, uint16_t len)` | schreibt **DS:0000..len**, also das komplette Datensegment, als Datei (Savegame-Dump!) | 2 | Spielzustand explizit serialisieren (DGROUP-Layout dokumentieren) |
| 4505:0060 (= 450b:0000) | `dos_disk_reset` | `void (void)` | `INT21/0D` Puffer flushen | 1 | entfällt (`fflush`) |
| 44f3:0009 | `dos_get_drive` | `int dos_get_drive(int far *out)` | `out[0]=1`, `out[1]` = aktuelles Laufwerk (`INT21/19`) | 1 | Stub |
| 44f5:000f / 44f5:0010 (= 44f6:0000) | leere Stubs | – | `retf` | 2 / 1 | entfällt |
| 44ff:0004 | `dos_set_drive` | `int dos_set_drive(int drive)` | `INT21/0E` | 1 | entfällt |

Hauptnutzer: Segment **1c74** (Ressourcen-/Speicherverwaltung des Spiels: `1c74:06a3` alloc,
`06c9` free, `00a1`/`0111` Ressource laden), 2a62, 1433, 1054.

---

## 7. Turbo-C-2.0-Laufzeitbibliothek (4516–45d0)

| Adresse | Name (TC) | Signatur | Zweck | Aufrufe | Port-Strategie |
|---|---|---|---|---|---|
| 4516:0004 | `__IOerror` | `int pascal __IOerror(int doserr)` | DOS-Fehler → `errno`/`_doserrno` (Tabelle `DS:17c6`) | 1 | entfällt |
| 4516:003f | (leer) | – | – | 0 | entfällt |
| 4519:0010 | `exit` | `void exit(int status)` | atexit-Tabelle rückwärts, Hooks `[1820]/[1824]/[1828]`, `_exit` | 2 | `exit()` |
| 451d:0003 | `atexit` | `int atexit(void (far *fn)(void))` | max. 32 Einträge. Registriert in main: `kbd_shutdown`, `timer_shutdown`, `vga_restore_mode`, `snd_shutdown` | 4 | `atexit()` bzw. explizites Shutdown |
| 4520:0002 | `malloc` | `void far *malloc(unsigned n)` | → `farmalloc((ulong)n)` | 6 | `malloc` |
| 4520:0203 | `farmalloc` | `void far *farmalloc(unsigned long n)` | Far-Heap (Paragraph-Blöcke, freie Liste) | 2 | `malloc` |
| 4520:0017 / 007f / 012f / 019d | Heap-Interna | – | Block aus Freiliste lösen / teilen / Heap erweitern / ersten Block anlegen | je 1–3 | entfällt |
| 4551:0002 / 0092 / 00d9 | `__brk`-Interna, `brk`, `sbrk` | – | DOS-Speicherblock anpassen | 0–3 | entfällt |
| 4566:0004 | `setblock` | `int setblock(unsigned seg, unsigned paras)` | `INT21/4A` | 1 | entfällt |
| 4568:0004 | `__longtoa` | `char far *__longtoa(long v, char far *buf, int radix, int sign, char hexA)` | Zahl → String | 0 direkt | `snprintf` |
| 4568:0088 | `itoa` | `char far *itoa(int v, char far *buf, int radix)` | | 1 | `snprintf("%d")` |
| 4568:00b5 | `ultoa` | `char far *ultoa(unsigned long v, char far *buf, int radix)` | | 0 | `snprintf` |
| 4568:00d5 | `ltoa` | `char far *ltoa(long v, char far *buf, int radix)` | | 1 | `snprintf("%ld")` mit `int32_t` |
| 4578:0000 | **`strlen`** | `size_t strlen(const char far *s)` | | **35** | `strlen` |
| 4579:000b | `strcpy` | `char far *strcpy(char far *d, const char far *s)` | | 2 | `strcpy` |
| 457c:0004 | `setmem` | `void setmem(void far *p, unsigned n, char v)` | | 2 | `memset` |
| 457c:002a | `memset` | `void far *memset(void far *p, int v, size_t n)` | Wrapper | 0 | `memset` |
| 4580:0009 | `farcalloc` | `void far *farcalloc(ulong n, ulong size)` | nullt in 64000-Byte-Stücken | 1 | `calloc` |
| 4589:000f | `free` | `void free(void far *p)` | → `farfree` | 7 | `free` |
| 4589:02e9 | `farfree` | `void farfree(void far *p)` | | 2 | `free` |
| 4589:0021 / 00c0 / 0138 / 021d | farfree-Interna | – | in Freiliste einfügen / verschmelzen / Heap-Top freigeben | je 1 | entfällt |
| 45bc:0003 | `abs` | `int abs(int)` | | 3 | `abs` |
| 45bd:0003 / 45bd:007a | `atoi` (Kern / Wrapper) | `int atoi(const char far *s)` | Whitespace-Tabelle `DS:183f` | 0 / 2 | `atoi` |
| 45c5:000d | `calloc` | `void far *calloc(size_t n, size_t size)` | | 3 (Huffman) | `calloc` |
| 45cc:0009 | `srand` | `void srand(unsigned seed)` | | 1 | **TC-LCG nachbauen** |
| 45cc:001a | `rand` | `int rand(void)` | `seed = seed*0x015A4E35 + 1; return (seed>>16) & 0x7FFF` | 1 | **exakt nachbauen** (nicht libc-`rand`), damit Zufallsfolgen identisch bleiben |
| 45d0:0002 | **`strncpy`** | `char far *strncpy(char far *d, const char far *s, size_t n)` | | **16** | `strncpy` |

Nicht verlinkt/benutzt: `printf`-Familie, `FILE*`-stdio, `LMOD@`, `LXRSH@`, BGI.
Die Text-Formatierung im Spiel (Strings wie `"#02d:#02d"`, `"#*13s"`, `"#s~#s"` in DGROUP)
läuft über einen **eigenen Formatierer** (vermutlich Segment 1d41, das `itoa/ltoa/strlen/
strncpy/atoi` intensiv nutzt) – `#` statt `%`, `~` als Sonderzeichen.

---

## 8. Ranking – meistgerufene Library-Funktionen

| Rang | Funktion | Call-Sites | Bedeutung für den Port |
|---|---|---|---|
| 1 | `LXMUL@` 1000:0412 | 79 | entfällt (32-Bit-Arithmetik) |
| 2 | `LDIV@` 1000:042b | 67 | entfällt |
| 3 | `bitmap_alloc` 44bc:000e | 65 | Kern-API Grafik |
| 4 | `scr_blit` 43de:0009 | 37 | Kern-API Grafik |
| 5 | `strlen` 4578:0000 | 35 | libc |
| 6 | `ticks_get` 42fe:0016 | 26 | Timing – SDL |
| 7 | `bmp_fill` 43e5:000d | 17 | Grafik |
| 8 | `scr_grab` 43d7:0006, `strncpy` 45d0:0002 | 16 | Grafik / libc |
| 10 | `hes_load` 4500:0006 | 15 | Datei-I/O |
| 11 | `PCMP@` 1000:03f1 | 14 | entfällt |
| 12 | `hes_read` 4510:0003 | 12 | Datei-I/O |
| 13 | `hes_filesize` 44ce:012c | 10 | Datei-I/O |
| 14 | `kbd_getkey` 44ef:000a, `ticks_set` 42fe:001a, `music_on_leave` 42a5:04dd | 8 | Eingabe / Timing / Sound |

---

## 9. Offene Fragen / Hinweise für den Port

1. **Tick-Rate:** `ticks_get` liest `DS:9014`. Der Zähler wird offenbar vom **PSM-Soundtreiber**
   im Timer-IRQ hochgezählt (Treiber-Slot 3 bekommt `&DS:900c`, Feld +8 = 9014). Beide Treiber
   programmieren den PIT selbst um (`out 43h,36h/34h`, Teiler dynamisch). Die genaue Frequenz
   muss aus `095.HES`/`098.HES` bestimmt werden. Ohne geladenen Treiber würden alle
   `while (ticks_get() < n)`-Warteschleifen hängen → Treiber ist faktisch Pflicht.
2. **Musik nur mit AdLib:** `music_on_enter` spielt nur, wenn `DS:1602 == 1` (AdLib erkannt).
3. **Savegame = DGROUP-Dump:** `hes_write_dgroup` schreibt das Datensegment ab Offset 0
   roh auf Platte. Für Savegame-Kompatibilität muss das DGROUP-Layout (Offsets) dokumentiert
   und im Port nachgebildet oder ein eigenes Format definiert werden.
4. **Relative Maus:** Maus wird jedes Mal auf 1600/1600 zurückgesetzt, das Spiel arbeitet nur mit
   Deltas (`DS:16a2/16a4`, Einheit = Mickey/8).
5. **Ghidra-Fallen:** Register-Parameter (`in_AX`, `in_CX`), falsche Rückgabewerte bei den
   Asm-Routinen (z. B. `mouse_poll`, `hes_*` geben `long` in DX:AX zurück), alle Treiber-Slots
   heißen `thunk_FUN_43ae_00c4`, Funktionen an Segmentgrenzen erscheinen doppelt
   (`43cb:0045`=`43cf:0005`, `44e4:00a8`=`44ee:0008`, `42a5:059a`=`42fe:000a`,
   `4505:0060`=`450b:0000`, `44f5:0010`=`44f6:0000`), `43d1:0000` und `5052:0000` liegen in
   Segmenten ohne bzw. mit falsch klassifizierter Funktion.
6. Die Blitter arbeiten ausschließlich in 8-Pixel-Spalten → Sprite-Positionen im Spiel sind
   meist 8-Pixel-ausgerichtet; nur die `*_shift`-Varianten erlauben Feinpositionierung.

# PALLADIX-PSM Soundtreiber – Spezifikation für die Neuimplementierung

Quellen: `098.HES` (AdLib-Treiber V1.3, 0x1565 Byte), `095.HES` (PC-Beeper V1.1, 0xe0f Byte),
`099.HES` (PSMDETECT), MTV.EXE Segmente `42a5`, `42fe`, `43ae`.
Disassembler-Hilfen: `re/tools/disasm_hes.py <HES> [start end]` (lineare Disassemblierung, Slot-Labels),
`re/tools/plx.py <re/v1> [nr ...] [-v]` (Musikdateien parsen/validieren, alle 24 Songs fehlerfrei).
Alle Adressen ohne Segment = Offset im Treiber (CS:0 = Dateianfang).

---

## 0. Kurzfassung

* **Spiel-Tick `DS:9014` läuft mit 50 Hz** (genau 1193182 / 11930 / 2 = 50,008 Hz), unabhängig vom
  Song-Tempo. Der Treiber erzeugt einen virtuellen 100-Hz-Takt (Akkumulator mit Teiler 0x2e9a) und
  zählt `9014` bei jedem 2. Takt hoch. Port: `ticks = (SDL_GetTicks() - t0) / 20` (u16).
* **Kein Fallback-Timer in MTV.EXE** (kein INT 8/1Ch-Hook, kein PIT-Zugriff außerhalb der Treiber).
* **Es wird praktisch immer der AdLib-Treiber geladen** (Detect-Logik, s. §1.3) – `DS:1602 == 1`
  immer. Der Beeper-Treiber ist im Spiel totes Material.
* Musik: 24 LZW-gepackte Dateien im **"PLX"-Format** (Typ 0 = 9 melodische OPL2-Kanäle), Event-Strom pro
  Kanal mit Bitmasken-Kommandos + 1-Byte-Delta, Instrumente = 12-Byte-OPL-Registersätze,
  Note = Index in F-Num/Block-Tabelle. Effekte (Pitch-Bends) sind als rohe F-Num-Events eingebacken.
  Keine Soundeffekte im Spiel (SFX-API existiert im Treiber, wird nicht benutzt).
* Musik-Tickrate = `1193182 / (tempo_divisor * speed)` Hz, typ. 6,5 … 13,3 Hz.

---

## 1. Laden, Installation, API

### 1.1 Dateiaufbau Treiber

```
+0x00  "PALLADIX-PSM\r\n\x1a$"            (16 Byte, vom Lader 43ae:00e5 als 16 Wörter verglichen)
+0x10  DOS-Stub (mov dx,100h; mov ah,9; int 21h; mov ax,4cffh; int 21h)
+0x1e  u16 count = 13
+0x20  u16 slot_off[13]                    Einsprünge, relativ zu CS:0
```

`psm_install` (43ae:00e5): Treiberpuffer muss paragraph-aligned sein (`para_align` 42a5:0582),
Offset wird in das Segment gefaltet (CS:0 = Dateianfang). Es patcht die 13 Far-Jumps bei
`43ae:0083 + 5*k` (Ghidra: alle `thunk_FUN_43ae_00c4`) und ruft Slot 0.
Nicht installierte Slots → Stub `43ae:00c4` = `mov ax,-1; cwd; retf`.

### 1.2 Aufrufkonvention

Alle Slots: **far call, C-Konvention (cdecl)**: Argumente als 16-Bit-Wörter auf dem
Stack, erstes Argument bei `[bp+6]`, Aufrufer räumt auf. Rückgabe `AX` bzw. `DX:AX` (far ptr).
Der Treiber sichert `DS, SI, DI, BP`; Zustand liegt im Treiber-CS. Viele Slots laufen mit `cli`.

**Besonderheit Slot 0:** `psm_install` pusht `p4,p3,p2,p1` und dann `sub sp,4` (zwei Dummy-Wörter) –
im Treiber liegt also `p1` bei `[bp+0xa]`, `p2` bei `[bp+0xc]` usw.

### 1.3 Detect (099.HES) und Treiberauswahl (snd_init 42a5:0002)

`099.HES` ab +0x24: `jmp 0x134` → AdLib-Test an Port 0x388 (klassischer Timer-1-Test:
`04←60, 04←80, 02←ff, 04←21`, Status `&e0 == c0`), Ergebnis AX = 3 (gefunden) / 0 (nicht gefunden)
wird in `rec[0].id` geschrieben, `rec[0]+0x48 = 0x388`. Rückgabe far ptr auf die Record-Tabelle (cs:0x30).

Record (0x50 Byte): `+0 i16 id (-1 = Tabellenende)`, `+2 i16 type (1 = AdLib, 0 = Beeper)`,
`+4 char file[..]` ("PSMADL.PSM"/"PSMBIP.PSM"), `+0x18 char name[]` ("Ad Lib System"/"Standard PC-Beeper"),
`+0x40 "PMA"/"PMB"`, `+0x44 "PNA"/"PNB"`, `+0x48..+0x4e` vier Init-Parameter (AdLib: 0x388,0,0,0; Beeper: 0,0,0,0).

| rec | id | type | +0x48 |
|---|---|---|---|
| 0 | 3 oder **0** | 1 (AdLib) | 0x388 |
| 1 | 3 | 0 (Beeper) | 0 |
| 2 | −1 | – | – |

`snd_init` sucht den ersten Record mit `id >= 0 && type == 1` (Assembler: `cmp es:[bx],0 / jge`).
Da der Detect bei Fehlschlag **0** (nicht −1) schreibt, wird **immer** der AdLib-Record gewählt →
`DS:1602 = 1`, `098.HES` wird geladen (0x1574 Byte alloziert), `psm_install(drv, 0x388, 0, 0, 0)`,
danach `slot5(0)` (Musik aus). Der Beeper-Zweig (095.HES, 0xe1e Byte) ist unerreichbar.

### 1.4 Slot-Tabelle AdLib (098.HES)

Interne Variablen (cs-relativ), im Folgenden mit Namen benutzt:

| cs: | Name | Bedeutung |
|---|---|---|
| 0420 | `pit_div` | aktueller PIT-Teiler (Init 0x2e9a) |
| 0422 | `acc100` | Akkumulator für 100-Hz-Zähler |
| 0424 | `acc_bios` | Akkumulator für Weiterleitung an alten INT 8 |
| 0426 | `snd_cnt` (i8) | Countdown bis zum nächsten Musik-Tick (in PIT-IRQs) |
| 0427 | `counters` (far ptr) | Zählerstruktur des Spiels (Slot 3) |
| 042b | `phase` (u8) | 100-Hz-Phasenzähler (für 50/25 Hz) |
| 042c | `tenth` (i8) | Countdown 9..0 für 10 Hz |
| 042d | `old_int8` (far) | gesicherter INT-8-Vektor |
| 0431 | `in_music` | Reentranz-Sperre |
| 07d0 | `shadow[256]` | Schattenkopie aller OPL-Register |
| 08d0 | `opl_defaults[256]` | Init-Werte (0xff = nicht schreiben), s. §3.7 |
| 09d0 | `last_reg` | zuletzt selektiertes OPL-Register |
| 09d1 | `mod_op[11]` | Modulator-Operator-Offset je Kanal |
| 09dc | `car_op[11]` | Carrier-Operator-Offset je Kanal |
| 09e7 | `note_tab[96]` (u16) | F-Num/Block-Tabelle |
| 0aa7 | `port` | 0x388 (Slot-0-Parameter p1) |
| 0aa9/0aab | `dly_idx/dly_dat` | kalibrierte Warteschleifen (Anzahl `in`) |
| 0aad | `bd_bit[11]` | Rhythmus-Bits (0×6, 10 08 04 02 01) |
| 0ab8 | `active` | Anzahl aktiver Kanäle |
| 0aba | `ch_ptr[11]` (u16) | Stream-Zeiger je Kanal (0 = inaktiv) |
| 0ad0 | `ch_vol[11]` (u8) | aktueller Carrier-Level (KSL\|TL) je Kanal |
| 0adb | `ch_next[11]` (u16) | absoluter Tick des nächsten Events |
| 0af1 | `perc_mode` | 1 = Rhythmus-Modus (Songtyp 1) |
| 0af2 | `is_sfx_bank` | 1 = geladene Datei ist SFX-Bank (Typ 2) |
| 0af3/0af5 | `song` (far) | Songdaten (Offset immer 0, Segment) |
| 0af7 | `song_time` | Tick-Zähler des Songs |
| 0af9 | `fade_rate` (i16) | 0 = kein Fade, >0 Ausblenden, <0 Einblenden |
| 0afb | `fade_acc` | Fade-Akkumulator (High-Byte = zusätzliche Dämpfung in TL-Schritten) |
| 0afd | `stopped` | 1 = Song gestoppt/beendet |
| 0afe | `muted` | 1 = per Slot 5 ausgeschaltet |
| 0aff | `tempo_div` | Tempo-Teiler (Header bzw. Tempo-Event) |
| 0b01 | `speed` | PIT-IRQs pro Musik-Tick |
| 0b02 | `loops_left` | Wiederholungen (Slot 6 setzt 0 → praktisch endlos) |
| 0b05 | `loop_counter` (far ptr) | wird bei jedem Songende inkrementiert (Slot 12) |
| 0b09 | `dispatch` | near ptr: 0ef5 (melodisch), 1046 (Rhythmus), 1252 (SFX) |
| 0b0b… 0b35 | SFX-Stimmenverwaltung (3 Stimmen), s. Slot 10 |

| k | Off | Name | Signatur | Wirkung | Aufruf im Spiel |
|---|---|---|---|---|---|
| 0 | 0040 | `drv_init` | `int (dummy,dummy,int port,p2,p3,p4)` | Zustände nullen (`active=0, song=0, stopped=0, muted=1`), `port=p1`; `opl_reset` (0d14: Delay-Kalibrierung über PIT-Kanal 2, Reg 01←20, alle `opl_defaults` schreiben); `timer_hook` (056d: INT 8 → cs:0432, PIT ← 0x2e9a). → 0 | `psm_install` (p1=0x388) |
| 1 | 0071 | `drv_shutdown` | `void ()` | INT 8 zurück, PIT-Teiler 0xffff (≈18,2 Hz), `opl_reset` | `psm_uninstall` 43ae:0164 (atexit `snd_shutdown` 42a5:02c0) |
| 2 | 040c | `drv_name` | `char far *()` | DX:AX → "V1.3 - AdLib (TM) soundcard" (cs:03f0) | – |
| 3 | 007e | `set_counters` | `void (void far *p)` | `counters = p` (unter cli). → AX = Segment | `ticks_set` 42fe:001a: `slot3(&DS:900c)` (7×, immer nach `ticks_set(0)`) |
| 4 | 0093 | `get_counters` | `void far *()` | DX:AX = counters | – |
| 5 | 00bf | `enable` | `int (int on)` | Status vorher berechnen; `muted = !(on&1)`; wenn jetzt muted → `opl_silence` (0c64); wenn on und `stopped==0` → `opl_restore_levels` (0ca0). → alter Status | `snd_init`: 5(0); `music_start` 42a5:028f: 5(1); `music_stop` 42a5:02a8: 5(0) |
| 6 | 00f3 | `load` | `int (void far *data, u16 len)` | `len` merken; `muted` sichern, =1, `opl_silence`; `song_parse(data)` (1431); `stopped = is_sfx_bank ^ 1` (Musik → gestoppt!); `loops_left=0`; `muted` zurück. → 0 bzw. −4 (Datei nicht paragraph-aligned oder Typ > 2) | `music_load_play` 42a5:01a8: `slot6(aligned_buf, unpacked_len)` |
| 7 | 0134 | `get_data` | `void far *()` | DX:AX = song | – |
| 8 | 013c | `get_len` | `u16 ()` | `len` aus Slot 6 | – |
| 9 | 0141 | `fade` | `int (int flags, u16 dur)` | Start/Stopp/Fade, s. §1.5. → neuer Status | `music_start`: 9(4,0) = sofort starten; `music_stop`: 9(2,0) = sofort stoppen |
| 10 | 01f8 | `sfx` | `int (int n, int arg)` | nur bei SFX-Bank (sonst −5); s. §1.6 | – |
| 11 | 01f4 | `status` | `int ()` | Status (s. u.) | – |
| 12 | 03a8 | `play_loops` | `int (void far *cnt, int loops)` | nur Musik (sonst −5): `loops_left = loops+1`, `loop_counter = cnt`, `*cnt = 0`, `stopped = 0`, Song neu parsen (startet von vorn) | – |

**Status-Wort** (0x9b): Bit0 = eingeschaltet (`muted==0`), Bit1 = `stopped`,
Bit2 = Einblenden aktiv (`fade_rate<0`), Bit3 = Ausblenden aktiv (`fade_rate>0`).

Fehlercodes: −1 Slot nicht installiert (Stub), −2 Installation fehlgeschlagen, −3 SFX-Nummer ungültig,
−4 Datei ungültig, −5 falscher Dateityp für diese Funktion.

### 1.5 Slot 9 `fade(flags, dur)`

```c
if (flags & 0x0c) {                       // Fade gewünscht (4 = ein, 8 = aus ... s.u.)
    flags &= 0x0c;
    T   = min(0xffff, tempo_div * (speed*64) / 11930);  // 64 * Dauer eines Musik-Ticks in 1/100 s
    r   = (dur && (T<<8)/dur < 0x3f00) ? (T<<8)/dur : 0; // Rate pro Musik-Tick (8.8 TL-Einheiten)
    if (r == 0) { if (!(flags & 4)) flags |= 2; goto immediate; }
    r = max(r,1); if (flags & 4) r = -r;               // 4 = Einblenden
    old = fade_rate; fade_rate = r;
    if (old == 0) fade_acc = (r<0) ? 0x3f00 : 0x0001;
    if (!(flags & 8)) { was = stopped; stopped = 0; if (was) opl_restore_levels(); }
} else {
immediate:
    fade_rate = fade_acc = 0;
    stopped = (flags & 2) >> 1;
    if (stopped) opl_silence(); else opl_restore_levels();
}
return status();
```
`dur` ist damit in **1/100 s**. Im Spiel nur: `fade(4,0)` → sofort spielen (stopped=0, Pegel
wiederherstellen), `fade(2,0)` → sofort stoppen + alle Kanäle stumm (TL=0x3f, Key-Off).

### 1.6 Slot 10 `sfx(n, arg)` (vom Spiel nicht benutzt – nur Überblick)

Nur wenn eine Datei vom Typ 2 (SFX-Bank) geladen ist. 3 SFX-Stimmen v=0..2, jede belegt OPL-Kanäle
3v..3v+2 mit eigenem Zeitzähler; Bankheader `[7] = Anzahl`, `[7+2n]` → SFX n = 3 Stream-Zeiger.
`n>0`: SFX n starten, `arg` Wiederholungen (freie oder älteste Stimme). `n<0`: SFX −n einreihen bzw.
Wiederholungen erhöhen (→ 1 wenn eingereiht). `n=0`: `arg=0` alle stoppen, `arg>0` SFX arg stoppen,
`arg<0` laufende ausklingen lassen (keine Wiederholung, Warteschlange leeren).
Event-Kodierung wie Musik (§3.4). Für den Port irrelevant.

### 1.7 Beeper-Treiber 095.HES (Kurzinfo)

Gleiche Slot-Semantik (Offsets: 0040, 006a, 02e5, 0077, 0092, 00ad, 00e1, 0122, 012a, 012f, 0153, 0157, 028c;
Name "V1.1 - PC-Beeper"). Gleiche Zählerstruktur und gleicher virtueller 100-Hz-Takt (cs:0302),
PIT Modus 2 (`out 43h,34h`), Slot 3 setzt zusätzlich den Akkumulator zurück, Slot 9 kennt kein Fade
(nur Bit 1/8 = stop). Wird im Spiel nie geladen (§1.3).

---

## 2. Timer

### 2.1 PIT-Programmierung

`pit_set(ax)` (054c): `if (ax==0) ax=0xffff; if (ax < 100) return;` sonst `pit_div = ax`,
`out 43h,36h` (Kanal 0, Modus 3), Teiler lo/hi nach Port 40h.

* Slot 0: `pit_set(0x2e9a)` → **100,015 Hz** (11930 Takte).
* `song_parse`: `pit_set(header.tempo_div)` (Songs: 4660…5920 → 201…256 Hz).
* Tempo-Event (§3.4, Bit 0x40): `pit_set(word)`.
* Slot 1: `pit_set(0xffff)`.

Die PIT-Rate ändert sich also mit dem Song; der Spieltakt bleibt dank Akkumulator konstant.

### 2.2 ISR (cs:0432, INT 8)

```c
isr() {
    acc100 -= pit_div;                          // u16, Borrow = 100-Hz-Takt fällig
    if (borrow) {
        do {
            if (counters) {
                c[0..3]++;                      // +0x00..+0x06: 100 Hz
                if (--tenth < 0) { c[12..15]++; tenth = 9; }   // +0x18..+0x1e: 10 Hz
                if ((++phase & 1) == 0) {
                    c[4..7]++;                  // +0x08..+0x0e: 50 Hz   <- DS:9014 = +0x08
                    if ((phase & 3) == 0) c[8..11]++;          // +0x10..+0x16: 25 Hz
                }
            }
            acc100 += 11930;                    // 0x2e9a
        } while (!carry);                       // so oft wie nötig (bei pit_div > 11930)
    }
    if (--snd_cnt < 0) {                        // Musik-Tick fällig?
        if (active==0 || song_seg==0 || stopped || muted) snd_cnt = 0;   // nichts zu tun
        else if (!in_music) {
            in_music = 1;
            acc_bios += pit_div; if (carry) call old_int8 (pushf/call far) else out(20h,20h); // EOI
            switch to private stack (cs:07c4), sti;
            music_tick();                       // 13aa
            snd_cnt += speed;                   // -> nächster Tick nach 'speed' IRQs
            restore stack; in_music = 0; iret;
        }
    }
    acc_bios += pit_div;                        // alten INT 8 mit ~18,2 Hz weiterbedienen
    if (carry) jmp far old_int8; else { out(20h,20h); iret; }
}
```
(Wenn `counters == 0` werden die Zähler übersprungen.)

### 2.3 Zählerstruktur des Spiels (`DS:900c`, 32 Byte, vom Treiber beschrieben)

| DS: | Feld | Rate |
|---|---|---|
| 900c, 900e, 9010, 9012 | c[0..3] | 100 Hz |
| **9014**, 9016, 9018, 901a | c[4..7] | **50 Hz** |
| 901c, 901e, 9020, 9022 | c[8..11] | 25 Hz |
| 9024, 9026, 9028, 902a | c[12..15] | 10 Hz |

Das Spiel benutzt nur `9014` (`ticks_get` 42fe:0016, 26 Aufrufe; `ticks_reset` 42fe:000e;
`ticks_set(n)` 42fe:001a = `9014 = n; slot3(&900c)`). Alle anderen Felder sind ungenutzt
(nicht in Savegames relevant außer als DGROUP-Bytes). `902c/902e` gehören nicht mehr dazu.

**Tick-Rate `DS:9014` = 1193182 / 11930 / 2 = 50,008 Hz (20 ms).**

### 2.4 Ohne Treiber

MTV.EXE enthält keinen eigenen Timer (keine INT-8/1Ch-Vektoren, kein `out 40h/43h`; die einzigen
`INT21/25xx` sind die TC-RTL-Vektoren 0/4/5/6). Ohne installierten Treiber liefert Slot 3 den Stub
(−1) und `9014` bleibt stehen → alle `while (ticks_get() < n)`-Schleifen hängen. Wegen §1.3 tritt das
im Original praktisch nie auf. **Port:** Tick-Zähler unabhängig vom Audio aus `SDL_GetTicks()` ableiten
(`(now-t0)/20`, bei `ticks_set(n)` `t0` so setzen, dass der Zähler `n` ergibt; u16-Überlauf beibehalten).

---

## 3. Musikformat "PLX" (nach LZW)

### 3.1 Container

HES-Datei: `+0 u16 unpacked_len`, `+2 u16 packed_len`, LZW-Daten ab +8 (`re/tools/lzw.py`).
`music_load_play(nr)` (42a5:01a8): liest `unpacked_len`, alloziert `len+15`, richtet auf Paragraph aus,
entpackt (438d:0002, Arbeitspuffer 0x2000), `slot6(buf, unpacked_len)`. Alle Offsets in der Datei sind
relativ zum Anfang der entpackten Daten (Treiber: DS = Songsegment, Offset 0).

### 3.2 Header

```
+0  char  magic[3] = "PLX"     (vom Treiber nicht geprüft)
+3  u8    type                 0 = melodisch (9 Kanäle), 1 = Rhythmus (6+5), 2 = SFX-Bank; >2 → Fehler −4
+4  u8    speed                PIT-IRQs pro Musik-Tick
+5  u16   tempo_div            PIT-Teiler (wird nur gesetzt wenn >= 100)
+7  u16   ch_ptr[9]            Typ 0: Startoffset je Kanal 0..8, 0 = Kanal unbenutzt
          (Typ 1: ch_ptr[11]; Typ 2: +7 u16 count, +9.. u16 sfx_ptr[count])
danach    Instrumente (12 Byte, ab 0x1d) und Event-Ströme
```

Musik-Tickrate: `f_tick = 1193182 / (tempo_div * speed)` (bei speed=0 wie speed=1).
Alle Spieldateien sind **Typ 0**.

### 3.3 Instrument (12 Byte, über Offset referenziert)

| Byte | Ziel |
|---|---|
| 0 | ungenutzt (Werte 0x00/0x10) |
| 1 | `C0+ch` (Feedback/Connection) |
| 2..6 | Modulator: `20+op, 40+op, 60+op, 80+op, E0+op` |
| 7..11 | Carrier: `20+op, 40+op, 60+op, 80+op, E0+op` |

Laden (0e12 → 0b8a): `ch_vol[ch] = inst[8]` (Carrier-KSL/TL), dann die 10 Operator-Register und `C0+ch`
schreiben (Reihenfolge: Mod 20,40,60,80,E0; C0; Car 20,40,60,80,E0).
Operator-Offsets (`mod_op`/`car_op`) Kanal 0..8: `00/03 01/04 02/05 08/0b 09/0c 0a/0d 10/13 11/14 12/15`.

### 3.4 Event-Strom (pro Kanal)

Ein Event = `cmd:u8 [Operanden] delta:u8`. Ausnahme: `cmd == 0` = Kanalende (kein Delta).
`cmd == 0x80` = nur Pause (Delta folgt). Sonst ist `cmd` eine **Bitmaske**, Operanden in dieser Reihenfolge:

| Bit | Operand | Aktion (Reihenfolge der Ausführung = Bitreihenfolge) |
|---|---|---|
| 0x01 | u16 inst_off | Instrument laden (§3.3) |
| 0x02 | u8 level | `ch_vol[ch] = level`; `set_level(ch, level)` |
| 0x04 | – | Key-Off: `B0+ch &= ~0x20` |
| 0x08 | u8 note | `w = note_tab[note/2]` (note ist Byte-Offset, immer gerade) |
| 0x10 | u16 freq | (nur wenn 0x08 nicht gesetzt) `w = freq` = roh `fnum \| block<<10` (Pitch-Bends/Slides) |
| 0x20 | – | Key-On |
| 0x40 | u16 div | Tempo: `tempo_div = div; pit_set(div)` (nur Musik, bei SFX-Bank Operand überlesen) |
| 0x80 | – | zusammen mit anderen Bits → ungültig → wie Kanalende |

Frequenz/Key-On (Bits 0x08/0x10/0x20, Routine 0e99):
```c
keyon = (shadow[B0+ch] | cmd) & 0x20;          // nach evtl. Key-Off (Bit 2) gelesen!
if (cmd & 0x08)      w = note_tab[note/2];
else if (cmd & 0x10) w = freq;
else                 w = shadow[A0+ch] | shadow[B0+ch]<<8;   // nur 0x20: Re-Key mit alter Frequenz
write(A0+ch, w & 0xff); write(B0+ch, (w>>8) | keyon);
```
Also: Bit 2 + Bit 5 = Note neu anschlagen; nur Bit 3/4 ohne Bit 2 = Legato (Key-On bleibt wie er war).
Beobachtete Kommandos: 02 03 04 06 07 08 10 12 20 22 23 24 26 27 28 2a 2b 2c 2e 2f 30 33 34 36 37 40 47 6b 6c 6e 6f 80.

`set_level(ch, v)` (0cb9): `write(40+car_op[ch], v)`; ist `shadow[C0+ch] & 1` (additiv), zusätzlich
`write(40+mod_op[ch], (shadow[40+mod_op] & 0xc0) | (v & 0x3f))`. `v` = KSL\|TL (0 = laut, 0x3f = stumm).

Delta: `ch_next[ch] += delta`. **Pro Musik-Tick und Kanal wird höchstens ein Event verarbeitet**
(Delta 0 wirkt wie 1; kommt in den Spieldateien nicht vor). Alle Kanäle eines Songs enden
(`cmd 0`) zum selben Tick.

### 3.5 Note-Tabelle (cs:09e7, 96 Einträge, Index = note/2)

`note_tab[i] = fnum[i % 12] | (i / 12) << 10` (Block 0..7), bereits im B0-Layout (High-Byte = `block<<2 | fnum>>8`).

| i%12 | C | C# | D | D# | E | F | F# | G | G# | A | A# | H |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| fnum | 157 | 16B | 181 | 198 | 1B0 | 1CA | 1E5 | 202 | 220 | 241 | 263 | 287 |

(hex; Beispiel note=0x56 → i=43 → Block 3, G, Wort 0x0E02.) In den Songs: note 24…170.

### 3.6 Player (Musik-Tick, 13aa + Dispatcher 0ef5)

```c
void music_tick(void) {                    // aus ISR, alle 'speed' PIT-IRQs
    if (active == 0) return;
again:
    restarted = 0;
    dispatch_melodic();
    if (restarted) goto again;             // Songneustart: Tick 0 sofort im selben Tick
    if (fade_rate) fade_step();
}

void dispatch_melodic(void) {
    int n = active;
    for (ch = 0; ch < 9 && n > 0; ch++) {
        if (!ch_ptr[ch]) continue;
        n--;
        if (ch_next[ch] > song_time) continue;
        p = ch_ptr[ch];
        cmd = *p++;
        if (cmd != 0x80 && !exec(ch, cmd, &p)) {      // exec false = cmd 0 / ungültig
            ch_ptr[ch] = ch_next[ch] = 0; active--;
            key_off(ch);                               // B0 &= ~0x20 (über Schatten)
            if (active == 0) { song_end(); return; }
            continue;
        }
        ch_next[ch] += *p++;
        ch_ptr[ch] = p;
    }
    if (++song_time == 0) song_end();
}

void song_end(void) {
    if (loop_counter) (*loop_counter)++;
    if (--loops_left != 0) { song_parse(song); restarted = 1; }   // Slot 6 setzt 0 → ~endlos
    else stopped = 1;
}

void song_parse(data) {                    // 1431
    is_sfx_bank = (type == 2); perc_mode = type & 1;
    write(0xBD, perc_mode ? 0x20 : 0x00);
    speed = data[4]; tempo_div = u16(data+5); pit_set(tempo_div);
    active = 0;
    for (ch = 0; ch < 9 /*11*/; ch++) {
        ch_ptr[ch] = u16(data + 7 + 2*ch); if (ch_ptr[ch]) active++;
        ch_vol[ch] = 0x3f; ch_next[ch] = 0;
    }
    song_time = 0;                         // Instrumente/OPL-Register werden NICHT zurückgesetzt
}

void fade_step(void) {                     // 13d3
    int att = fade_acc >> 8;
    for (ch = 0; ch < 11; ch++) {
        v = ch_vol[ch];
        if ((v & 0x3f) == 0x3f) continue;
        tl = (v & 0x3f) + att; if (tl > 0x3f) tl = 0x3f;
        set_level(ch, (v & 0xc0) | tl);    // ch_vol bleibt unverändert
    }
    if (fade_acc == 0)            fade_rate = 0;                         // Einblenden fertig
    else if (fade_acc > 0x3f00) { fade_rate = 0; stopped = 1; opl_silence(); }
    else { fade_acc += fade_rate; if ((int16_t)fade_acc < 0) fade_acc = 0; }
}
```

Hilfsroutinen:
* `opl_silence` (0c64): bei Rhythmus `BD←20`; für jeden Kanal Key-Off (`B0`, falls gesetzt) und
  `set_level(ch, ch_vol[ch] | 0x3f)`.
* `opl_restore_levels` (0ca0): `set_level(ch, ch_vol[ch])` für alle Kanäle (9 bzw. 11).
* OPL-Schreiben (0b3e): Wert wird gegen `shadow[]` verglichen und nur bei Änderung geschrieben;
  Index nur neu selektiert wenn `reg != last_reg`. Port `0x388`/`0x389`, Wartezeiten durch kalibrierte
  `in al,dx`-Schleifen. **Für den Port: Schattenregister nachbilden** (Key-On-Logik liest `shadow[B0]`,
  `set_level` liest `shadow[C0]`/`shadow[40+mod]`); Schreiben an den Emulator kann ungefiltert erfolgen.

### 3.7 OPL-Initialisierung (`opl_reset` 0d14, auch bei Shutdown)

Reg 01←20 (Waveform-Select an), dann alle Register von 0xff abwärts aus `opl_defaults`
(Werte ≠ 0xff):
`01=20 02=00 03=00 04=00 08=40`, `20..35=00`, `40..55=3f`, `60..75=ee`, `80..95=ee`, `A7=57 A8=02`,
`B0..B6=1f B7=09 B8=0e`, `BD=00`, `C0..C8=00`, `E0..F5=00` (nur gültige Operator-Adressen).
Danach ist `shadow[]` = diese Werte (Rest 0xff bzw. unverändert).

### 3.8 Rhythmus-Modus (Typ 1, im Spiel unbenutzt)

11 Kanäle: 0..5 melodisch wie oben; 6 = BD (beide Operatoren Kanal 6), 7 = SD (Op 0x14),
8 = TOM (Op 0x12), 9 = CYM (Op 0x15), 10 = HH (Op 0x11); `bd_bit = 10 08 04 02 01`.
Kommandos für Kanal 6..10 (0f6e): Bit0 Instrument (nur ein Operator bei 7..10), Bit1 Level,
Bit2 = Drum-Bit in BD löschen, Bit 0x08/0x10 Frequenz nur für Kanal 6 und 8 (bei 8 wird Kanal 7
auf ≈ 1,5 × fnum mit Blockkorrektur gesetzt; sonst Operanden überlesen), Bit 0x20 = Drum-Bit setzen,
0x40 Tempo.

---

## 4. Songs und Räume (Segment 42a5)

### 4.1 Ablauf im Spiel

* `room_enter(id)` (1054:0d49): `owner = Türtabelle`, `music_on_leave(id, owner)` (04dd),
  Ressourcen frei, `music_on_enter(id, owner)` (02c6) (bei Raum 8 erst nach `room_load`).
  Raum verlassen/zurück (1054:0bfc ff.) analog mit dem Zielraum. Zusätzlich `02c6(cur_room, 0)` nach
  Schließen eines Unterdialogs (decompiled.c Z. 3327).
* Intro: `8b88=1; music_on_enter(500,0)` → Song 149; nach `intro_play` `music_on_leave(500,0)`.
* `music_on_enter`: wählt Song `DS:1604`, dann `music_load_play(nr)` + `music_start()`
  (= `slot5(1); slot9(4,0)`). Song läuft in Endlosschleife.
* `music_on_leave`: wenn Musik gewechselt werden muss und `1604 != 0`: `music_stop()`
  (= `slot9(2,0); slot5(0)`), `music_free()`, `1604 = 0`.

Variablen: `1602` = AdLib (immer 1), `1604` = aktueller Song (0 = keiner), `1606` = Raum des letzten
`music_on_enter`, `1608` = Raum des letzten `music_on_leave`, `8b88` = aktueller Spieler (1-basiert),
`79f2` = Optionsschalter (Optionsdialog, Zeile y=0x54; vermutlich „Musik aus“).

### 4.2 `music_on_enter(room, owner)` – Bedingungen

Musik bleibt unverändert (nur `1606 = room`), wenn eine der Bedingungen gilt:
```
79f2 != 0
|| (owner == player && (room == 0x19 || room == 0x3a || (room == 1 && 1606 != 0 && 1606 != 0x1f)))
|| (room == 0x0a && 1606 != 0)
|| (owner != 0 && owner != player && room != 0 && 1606 != 0)
|| room in {0x0d, 0x3d, 0x3e, 0x40}
|| (room == 0 && 1606 in {0x0d, 0x3d, 0x40})
```
Sonst `1606 = room`, Song bestimmen (nur wenn `1602 == 1`), laden und starten (falls ≠ 0).

`music_on_leave(room, owner)`: Musik läuft weiter (nur `1608 = room`), wenn
```
(owner == player && (room == 0x19 || room == 0x3a || (room == 1 && 1608 != 0 && 1608 != 0x1f)))
|| room == 0x3e || (room == 0x0a && 1608 != 0)
|| (owner != 0 && owner != player && room != 0 && 1608 != 0)
|| room in {0x0d, 0x3d, 0x40} || (room == 0 && 1608 in {0x0d, 0x3d, 0x40})
```
sonst Musik stoppen/freigeben.

### 4.3 Raum → Song

| Bedingung | Raum-ID | Song (HES dez.) | Tick-Hz | Länge |
|---|---|---|---|---|
| room == 0 | 0x00 Hochhaus/Flur | **255** (0xff) | 7,01 | 110 s |
| eigener Raum (owner == player) | 0x01 | 97 | 8,68 | 148 s |
| | 0x02 | 87 | 8,01 | 138 s |
| | 0x03 | 96 | 8,01 | 144 s |
| | 0x04 | 82 | 8,01 | 112 s |
| | 0x05 | 78 | 6,67 | 228 s |
| | andere | – (keine Musik) | | |
| owner == 0 (neutral) | 0x08 | 79 | 10,68 | 165 s |
| | 0x09 | 147 | 5,35 | 42 s |
| | 0x0a | 94 | 6,50 | 148 s |
| | 0x0b | 88 | 8,01 | 36 s |
| | 0x0c | 81 | 8,68 | 116 s |
| | 0x0e | 92 | 7,34 | 79 s |
| | 0x10 | 80 | 8,01 | 140 s |
| | 0x11 | **254** (0xfe) | 8,01 | 32 s |
| | 0x12 | 89 | 8,00 | 96 s |
| | 0x13 | 90 | 8,01 | 40 s |
| | 0x14 | 85 | 8,01 | 60 s |
| | 0x15 | 84 | 6,67 | 43 s |
| | 0x16 | 148 | 8,01 | 36 s |
| | 0x17 | 83 | 8,01 | 56 s |
| | 0x18 | 91 | 10,68 | 54 s |
| | 0x42 | 86 | 9,34 | 124 s |
| | 500 (Intro/Titel) | 149 | 13,34 | 178 s |
| | andere | – | | |
| Raum eines Mitspielers (owner ≠ 0, ≠ player) | alle (≠0) | 93 | 6,50 | 81 s |

Damit sind alle 24 PLX-Dateien belegt: 078–094, 096, 097, 147, 148, 149, 254, 255
(095 = Beeper-, 098 = AdLib-Treiber, 099 = Detect). Header-Werte und Tempo-Events: `plx.py`.

---

## 5. Port-Empfehlung

1. **Ticks:** `ticks_get()` = `(uint16_t)((SDL_GetTicks() - t0) / 20)`; `ticks_set(n)`/`ticks_reset()` setzen `t0`.
2. **Audio:** OPL2-Emulator (z. B. Nuked-OPL3 im OPL2-Modus / DOSBox-DBOPL) im SDL-Audio-Callback;
   Sequenzer im Callback samplegenau takten: `samples_per_tick = rate * tempo_div * max(speed,1) / 1193182`
   (bei Tempo-Event neu berechnen). Pro Tick `music_tick()` aus §3.6, OPL-Writes direkt an den Emulator,
   Schattenregister wie im Treiber.
3. **API:** `snd_init` → OPL-Reset (§3.7), `music_load_play(nr)` → LZW + `song_parse`, `stopped=1`;
   `music_start` → `muted=0; stopped=0; restore_levels`; `music_stop` → `stopped=1; muted=1; silence`.
   Mutex/Lock zwischen Spielthread und Audio-Callback.
4. Fade, SFX, Rhythmus-Modus und Beeper werden vom Spiel nicht benutzt.

## 6. Offene Punkte

* Bedeutung von `DS:79f2` (Optionsschalter) und Instrument-Byte 0 nicht verifiziert.
* Raumnamen zu den IDs (Türtabelle `0x7ca8`) – s. architecture.md.
* Verhalten bei Delta 0 (wirkt als 1 Tick) – kommt in den Daten nicht vor.

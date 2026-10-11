# Übersetzungsrichtlinien MTV.EXE → C

Ziel: lesbarer, nativer C11-Code, der sich **genau wie das Original** verhält. Grundlage ist
der Ghidra-Export (`re/out/decompiled.c`), bei Zweifeln der Assembler (`re/scripts/DumpAsm.java`).

## Aufbau

| Pfad | Inhalt |
|---|---|
| `src/game/seg_XXXX.c` | ein Originalmodul (= ein Code-Segment von MTV.EXE) |
| `src/game/game.h` | Prototypen aller Spielfunktionen (generiert aus `re/notes/sigs/*.tsv`) |
| `src/game/globals.h` | benannte globale Variablen im Datensegment |
| `src/rtlib.h` | Ersatz der Assembler-Bibliothek und der Turbo-C-Laufzeit (Original-API) |
| `src/dgroup.h` | Datensegment `g_ds` und Zugriffsmakros `DS_*` |

Jede Funktion steht in der Datei ihres Originalsegments, in Adressreihenfolge, mit Kopfkommentar:

```c
/* 1f51:38b1 – zeichnet das Schedule-Raster (8 Spalten à 0x61 Zeilen) */
void draw_schedule_grid(void)
{
```

## Typen

| Original (Turbo C, 16 Bit) | Port |
|---|---|
| `int`, `short` | `int16_t` |
| `unsigned` | `uint16_t` |
| `long` / `unsigned long` | `int32_t` / `uint32_t` |
| `char` / `unsigned char` | `char` bzw. `int8_t` / `uint8_t` |
| far-Zeiger | normaler C-Zeiger (Build ist 32 Bit, Zeiger = 4 Byte wie im Original) |

Niemals `int`/`long` ohne Breite verwenden. **16-Bit-Überlauf:** C rechnet intern mit 32 Bit.
Wo das Original überlaufen kann und das Ergebnis vor dem Speichern verglichen oder geteilt wird,
explizit nach `(int16_t)`/`(uint16_t)` casten. Vorzeichen beachten (`JL/JG` = signed, `JB/JA` = unsigned).

## Globale Variablen

Alle globalen Variablen liegen im Datensegment `g_ds` (Layout wie im Original, wichtig für
Spielstände und initialisierte Tabellen). Zugriff:

- benannt, wenn in `globals.h` vorhanden: `g_cur_room`
- sonst über die Makros: `DS_I16(0x8312)`, `DS_U8(0x24e8)`, `DS_I32(0x71d5)`,
  Zeigervariablen `DS_PTR(Bitmap, 0x7bdc)`, Adressen/Arrays `DS_ADDR(int16_t, 0x43f7)`
- Array-Zugriff mit Element-Index: `DS_ADDR(int16_t, 0x43f7)[player * 0x2d + i]`

Neue Namen nicht in fremden Dateien vergeben, sondern im Report melden (zentrale Pflege).
Strings aus dem Datensegment: `DS_ADDR(char, 0x1398)` (Text als Kommentar dazuschreiben).

## Funktionen und Aufrufe

- Namen und Signaturen exakt wie in `game.h`. Ist eine Signatur falsch, nicht lokal ändern,
  sondern im Report melden.
- Ghidra-Artefakte entfernen: `push cs`-Phantomparameter (z. B. `0x1f51`), `CONCAT22`,
  `(ulong)x >> 0x10`-Zerlegungen von Far-Pointern, `unaff_DS`, `in_AX`, `extraout_*`.
- Turbo-C-Hilfen entfallen: `LXMUL@`→`*`, `LDIV@`→`/`, `LMOD@`→`%`, `SCOPY@`→Struct-Zuweisung bzw. `memcpy`,
  `PADA@/PADD@/PCMP@` → normale Zeigerarithmetik.
- Bibliotheksaufrufe → `rtlib.h`: `malloc`→`tc_malloc`, `free`→`tc_free`, `rand`→`tc_rand`,
  `srand`→`tc_srand`, `itoa/ltoa`→`tc_itoa/tc_ltoa`, `atoi`→`tc_atoi`, `exit`→`tc_exit`,
  `atexit`→`tc_atexit`, `strlen/strcpy/strncpy/memset/memcpy` → Standard-C.
- Blitter mit Parameterblock: Block als Struct aus `rtlib.h` (`BlitRectArgs` usw.), auch wenn
  das Original ihn im Datensegment ablegt: `bmp_blit_rect(DS_ADDR(BlitRectArgs, 0x8f2a))`.
- Soundtreiber-Slots (`thunk_FUN_43ae_00c4` an Adresse 43ae:0083+5k) → `psm_slot_k(...)`
  aus `sound.h` (k = 0..12).

## Kontrollfluss

- `switch` statt Sprungtabellen; `goto` nur, wenn der Originalfluss sonst nicht abbildbar ist.
- Schleifen wie im Original; keine Optimierungen, keine „Verbesserungen“ der Spiellogik.
- Bugs des Originals bleiben erhalten und werden mit `/* ORIGINAL-BUG: … */` markiert.

## Kommentare

Deutsch, knapp: was die Funktion im Spiel bewirkt. Unklare Stellen mit `/* UNKLAR: … */`.
Keine Kommentare, die nur den Code nacherzählen.

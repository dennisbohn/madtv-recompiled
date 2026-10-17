# Finanzen

## Start

Jeder Sender beginnt mit **500.000** und einem Sendemast, der rund 12 Mio. Zuschauer erreicht,
sowie einem kleinen Studio. Sterne auf der Kommandozeile (`E ***`) vervielfachen das Startkapital
der Computergegner.

## Tägliche Kosten

Zum Tagesbeginn (ab Tag 2) werden abgebucht:

| Posten | Höhe |
|---|---|
| Sender-Gebühren | 20 % des Kaufpreises aller Masten, siehe [Sendemasten](Sendemasten.md) |
| Studio-Mieten | je Studio, siehe unten |
| Nachrichten-Abos | 1.000 / 10.000 / 10.000, siehe [Nachrichten](Nachrichten.md#abos) |
| Kreditzinsen | 5 % des Kredits |

Studiomieten pro Tag (Startwerte): Größe 1 kostet 2.000; Größe 2 kostet 5.000, 8.000 oder 9.000;
Größe 3 kostet 8.000 oder 14.000. Ein vermietetes Studio kann man dem Mieter abkaufen.

Einnahmen gibt es fast nur aus [Werbung](Werbung.md) (Prämie bei Vertragserfüllung) und dem
Verkauf von Filmen und Masten.

**Verfügbares Geld** ist der Kontostand minus heute vorgemerkte Masten und eigene offene
Auktionsgebote.

## Kredit (beim Chef)

| Image | erster Kredit | einmalige Erhöhung | mit Sammy |
|---|---|---|---|
| unter 10 | keiner, Rauswurf aus dem Büro | – | – |
| 10–49 | 200.000 | +100.000 (ab Tag 2) | +200.000 |
| ab 50 | 400.000 | +200.000 | +400.000 |

Erhöhen lässt sich nur, solange der Kredit höchstens beim ersten Betrag liegt. Wer bescheiden
fragt, bekommt die kleine Erhöhung; die große gibt es nur, wenn man an diesem Tag den Sammy
gewonnen hat. Zurückgezahlt wird 100.000, 200.000 oder alles, jeweils höchstens der Kontostand
und abgerundet auf 50.000. Zinsen: **5 % pro Tag**.

## Gerichtsvollzieher

Liegt der Kontostand zum Tagesbeginn unter **−100.000**, kommt der Gerichtsvollzieher. Er pfändet
einen Film aus dem Archiv (gutgeschrieben wird der halbe Filmpreis) oder die laufende Produktion
in einem Studio.

---
Im Code: `daily_costs_pay`, `loan_interest_pay`, `event_npc_init`, `event_npc_strike`
(src/game/seg_2bd1.c), `day_begin` (src/game/seg_1054.c), `money_available` (src/game/seg_2a62.c),
`room_click_boss` (src/game/seg_19e6.c); Startwerte aus 208.HES.

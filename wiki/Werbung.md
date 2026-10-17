# Werbung

**Was man im Spiel sieht:** alles, was im Vertrag steht (Tooltip des Werbekunden). **Intern**
sind nur die Regeln, welche Kunden die Agentur anbietet und wann man unterschreiben darf.

## Vertrag

Jeder Werbekunde nennt im Tooltip:

- **Mindestzuschauer** – eine der Stufen 0 / 250.000 / 500.000 / 1 Mio. / 2,5 Mio. / 5 Mio. /
  10 Mio. / 25 Mio. / 50 Mio. / 100 Mio.
- **Anzahl Spots** und **Frist in Tagen**
- **Prämie** (bei Erfüllung) und **Strafe** (bei Fristablauf)
- eventuell Genres, in denen der Spot laufen muss bzw. nicht laufen darf ("Bitte …",
  "Bitte keine …")

## Unterschreiben

- Höchstens 10 laufende Verträge.
- Das Image muss mindestens so hoch sein wie die Prozentzahl, die im Tooltip hinter den
  Mindestzuschauern steht (das ist die nötige Quote bei der eigenen Reichweite). Manche Kunden
  verlangen zusätzlich Image 20; welche, zeigt das Spiel nicht an – man bekommt nur die Absage
  "Aufgrund Deines Images wird Dir der Spot verweigert".
- Intern: Die Agentur bietet nur Kunden an, deren Mindestzuschauer unter 40 % der größten
  Reichweite aller Sender liegen. Große Kunden erscheinen also erst, wenn irgendein Sender
  entsprechend gewachsen ist. Nicht unterschriebene Angebote verschwinden am nächsten Tag.

## Senden

Um Minute 55 wird der geplante Spot geprüft; im Sendeplan sieht man danach, ob er gelaufen ist:

- Mindestzuschauer 0: es muss nur überhaupt ein Programm laufen.
- sonst: die Zuschauer der Stunde, auf 100.000 gerundet, müssen die Mindestzahl erreichen.
- Das Genre der laufenden Sendung muss erlaubt sein.

Klappt es, zählt der Spot. Mit dem letzten nötigen Spot ist der Vertrag erfüllt, die **Prämie
wird sofort gutgeschrieben** und weitere eingeplante Spots dieses Kunden werden entfernt. Ein
Spot, der die Bedingungen nicht erfüllt, zählt nicht, kostet aber auch nichts.

## Frist

Erreicht der Tag die Frist, ohne dass alle Spots gelaufen sind, wird die **Strafe** abgezogen und
der Vertrag gelöscht. Ein Kunde ohne Strafe entzieht nur den Auftrag.

Werbeblöcke ohne eingeplanten Spot zählt der Chef und spricht sie am nächsten Tag an.

---
Im Code: `ad_market_update` und `ratings_calc` Phase 3 (src/game/seg_3264.c), `ad_contracts_expire`
und `ad_genre_allowed` (src/game/seg_2bd1.c), `room_click_ad_agency` (src/game/seg_1581.c);
Mindestzuschauer-Stufen DS:00a8 in src/game/ds_init.c.

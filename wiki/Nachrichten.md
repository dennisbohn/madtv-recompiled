# Nachrichten

## Wert einer Meldung

Jede Meldung hat einen Wert von etwa 0 bis 100. Er entsteht aus dem Grundwert des Ereignisses und
der Qualität der zufällig gewählten Textbausteine: **30 % bis 100 % des Grundwerts**.

- Zu jeder vollen Stunde fällt der Wert auf **85 %**.
- Zum Tageswechsel wird er **0** – Meldungen von gestern sind wertlos.

## Preis

Der Preis zeigt nur, in welcher Stufe der *aktuelle* Wert liegt:

| Wert | Preis |
|---|---|
| bis 15 | 1.000 |
| 16–30 | 2.000 |
| 31–40 | 3.000 |
| 41–50 | 4.000 |
| 51–60 | 6.000 |
| 61–100 | 8.000 |

Eine Meldung, die liegen bleibt, wird also von Stunde zu Stunde billiger. Zwei Meldungen für je
8.000 können zwischen 61 und 100 liegen; der Preis verrät nicht, welche besser ist.

## Qualität der Sendung

Die Sendung hat drei Plätze. Ihre Qualität ist der **Durchschnitt der drei Werte**; ein leerer
Platz zählt als 0 und kostet damit ein Drittel. Daraus wird mit dem Faktor der Stunde die
Nachrichtenquote (siehe [Quoten](Quoten.md#nachrichten-minute-0)). Außerdem bekommt das Programm
der Stunde ein Fünftel der Qualität als Bonus auf seinen Wert.

Teurere Meldungen bringen also mehr Zuschauer – aber drei mittlere Meldungen schlagen oft zwei
teure und einen leeren Platz, und frisch gesendet ist besser als lange gelagert.

## Abos

Neue Meldungen bekommt man nur aus abonnierten Kategorien. Abos kosten täglich und werden
gekündigt, wenn das Image unter die Schwelle fällt:

| Kategorie | Kosten pro Tag | nötiges Image |
|---|---|---|
| PolitNews | 1.000 | 5 |
| Showbusiness | 10.000 | 10 |
| Vermischtes | 10.000 | 15 |

Das eigene Nachrichtenarchiv fasst 20 Meldungen; kommt eine neue dazu, fällt die älteste heraus.

---
Im Code: `news_generate`, `newscast_rate`, `news_values_decay`, `news_distribute`
(src/game/seg_2a62.c), `genre_value_get`, `daily_costs_pay`, `news_subscriptions_check`
(src/game/seg_2bd1.c); Tabellen in src/game/ds_init.c (DS:11e6, DS:129a, DS:12a6).

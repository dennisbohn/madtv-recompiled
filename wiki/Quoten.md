# Quoten

**Was man im Spiel sieht:** Quote und Zuschauer der gesendeten Blöcke und die Aktualität der
Filme als Balken. **Alles andere auf dieser Seite rechnet das
Spiel intern** – Programmwert, Genre-Grenzen, Nachrichtenbonus und das Thema des Tages werden
nirgends angezeigt.

## Programm (ab Minute 5)

Für jeden Sender wird intern ein **Wert** der laufenden Sendung bestimmt:

1. **Grundwert:** die aktuelle Aktualität des Films bzw. der Serienfolge (siehe
   [Filme und Serien](Filme-und-Serien.md)). Bei Serien wird der Wert durch die Zahl geteilt, wie
   oft dieselbe Serie heute schon (einschließlich jetzt) gelaufen ist – die dritte Folge am
   selben Abend bringt nur noch ein Drittel.
2. **Thema des Tages:** passt das Genre, wird der Wert mit 1,3 bzw. 1,5 multipliziert (siehe unten).
3. **Genre und Uhrzeit:** jedes Genre hat je Stunde eine Obergrenze (Tabelle unten). Senden
   mehrere Sender dasselbe Genre und überschreiten zusammen die Grenze, wird anteilig gekürzt.
   Danach wird der Wert mit *Grenze / 100* multipliziert.
4. **Nachrichtenbonus:** war der Wert nicht 0, kommt ein Fünftel der Qualität der eigenen
   Nachrichtensendung dieser Stunde dazu.

Die **Quote** ist dann der eigene Wert in Prozent – solange die Werte aller Sender zusammen
höchstens 95 ergeben. Liegt die Summe darüber, werden alle anteilig gekürzt, sodass die Quoten
zusammen 95 % ergeben. Ein leerer Sendeplatz hat den Wert 0.

Zuschauer = eigene Reichweite × Quote. Ein Sender mit doppelter Reichweite hat bei gleicher
Quote also doppelt so viele Zuschauer.

### Genre-Grenzen je Stunde

Intern, im Spiel nicht einsehbar. Startwerte aus den Spieldaten (in Prozent):

| Genre | 18 | 19 | 20 | 21 | 22 | 23 | 0 Uhr |
|---|---|---|---|---|---|---|---|
| Lovestory | 35 | 40 | 85 | 80 | 70 | 50 | 45 |
| Actionfilm | 25 | 30 | 50 | 70 | 75 | 55 | 50 |
| Monumentalfilm | 20 | 25 | 85 | 80 | 75 | 50 | 30 |
| Komödie | 45 | 60 | 90 | 85 | 80 | 60 | 55 |
| Krimi | 75 | 70 | 60 | 55 | 60 | 60 | 55 |
| Kultur | 35 | 45 | 60 | 65 | 80 | 70 | 60 |
| Live-Übertragung | 75 | 80 | 95 | 90 | 80 | 65 | 60 |
| Show | 80 | 80 | 85 | 80 | 50 | 30 | 25 |
| Science Fiction | 25 | 35 | 85 | 80 | 75 | 65 | 60 |
| Sonstiges | 35 | 40 | 85 | 80 | 70 | 50 | 45 |
| Familienserie | 95 | 85 | 75 | 55 | 25 | 20 | 15 |
| SF-Serie | 85 | 80 | 60 | 40 | 45 | 60 | 30 |
| Western-Serie | 75 | 70 | 35 | 40 | 60 | 60 | 45 |
| Krimi-Serie | 80 | 75 | 60 | 55 | 65 | 60 | 55 |
| Sonst. Serie | 85 | 75 | 70 | 65 | 55 | 45 | 35 |

Faustregel: Serien und Shows früh am Abend, große Filme zwischen 20 und 22 Uhr.

### Thema des Tages

Ein unsichtbarer Trend: bestimmte Genres sind gerade beim Publikum gefragt und bekommen einen
Bonus. Das Spiel kündigt das nicht an; man merkt es nur an ungewöhnlich hohen Quoten – oder
über die Sendung "Im Trend" (siehe unten).

Jeden Tag zieht das Spiel ein neues Genre (nie Sonstiges oder Sonst. Serie, nie eines, das
gerade im Trend ist). Es wirkt ab dem folgenden Tag, drei Tage lang:

| Tag nach der Ziehung | Stufe | Bonus |
|---|---|---|
| 1. | Zunehmend gefragt | × 1,3 |
| 2. | Besonders beliebt | × 1,5 |
| 3. | Immer noch aktuell | × 1,3 |

Weil jeden Tag ein Genre nachrückt, sind immer drei Genres gleichzeitig im Trend, je eines pro
Stufe. In den ersten Spieltagen ist die Liste noch leer.

Beispiel: Montag wird Krimi gezogen, Dienstag Komödie, Mittwoch Western-Serie. Am Donnerstag
gilt dann Western-Serie × 1,3, Komödie × 1,5, Krimi × 1,3; am Freitag ist Krimi heraus.

**Sichtbar machen:** Die Drehbuchagentur hat, solange sie frei ist, immer die Show *"Zuschauer-Befragung zu aktuellen
Programm-Wünschen"* im Angebot. Produziert und gesendet ergibt sie die Sendung *"Im Trend"*;
danach bekommt der Sender eine Mitteilung mit den drei aktuellen Genres ("Besonders beliebt …,
Zunehmend gefragt …, Immer noch aktuell …"). Sendet ein Computergegner sie, erfährt nur er
die Trends.

## Nachrichten (Minute 0)

Die Qualität der eigenen Nachrichtensendung (intern, siehe [Nachrichten](Nachrichten.md)) wird
mit dem Faktor der Stunde multipliziert; das ist die Quote, solange die Qualitäten aller Sender
zusammen höchstens 95 ergeben (sonst anteilig gekürzt wie beim Programm). Die Faktoren sind
intern:

| 18 | 19 | 20 | 21 | 22 | 23 | 0 Uhr |
|---|---|---|---|---|---|---|
| 65 % | 70 % | 85 % | 80 % | 70 % | 60 % | 55 % |

## Image

Nach jeder Programmquote werden die Sender intern nach Quote sortiert: Platz 1 bekommt +1 Image,
Platz 3 −1. Bei Gleichstand ändert sich nichts. Ein Skandalfilm (FSK 18) vor 21 Uhr zählt dabei
als Quote 0. Mehr in [Image und Chef](Image-und-Chef.md).

---
Im Code: `ratings_calc` (src/game/seg_3264.c), `ratings_rank_update` und `newscast_quality_sum`
(src/game/seg_2bd1.c), `daily_topic_pick` (src/game/seg_3264.c); Tabellen DS:7140 und DS:1ce0
kommen aus 208.HES, die Trendstufen aus DS:0094 (src/game/ds_init.c). "Im Trend" ist Programm
Nr. 138, die Show "Zuschauer-Befragung" Nr. 24 (`SHOW_VIEWER_POLL`).

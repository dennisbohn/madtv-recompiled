# Quoten

## Programm (ab Minute 5)

Für jeden Sender wird ein **Wert** der laufenden Sendung bestimmt:

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

Startwerte aus den Spieldaten (in Prozent):

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

Jeden Tag wird ein neues Genre gezogen (nie Sonstiges oder Sonst. Serie, nie eines der letzten
drei). Ein Genre ist drei Tage lang im Trend:

| Stufe | Bonus |
|---|---|
| Zunehmend gefragt | × 1,3 |
| Besonders beliebt | × 1,5 |
| Immer noch aktuell | × 1,3 |

Die Show *Zuschauerumfrage* verrät die drei aktuellen Genres ("Im Trend").

## Nachrichten (Minute 0)

Die Qualität der eigenen Nachrichtensendung (siehe [Nachrichten](Nachrichten.md)) wird mit dem
Faktor der Stunde multipliziert; das ist die Quote, solange die Qualitäten aller Sender
zusammen höchstens 95 ergeben (sonst anteilig gekürzt wie beim Programm).

| 18 | 19 | 20 | 21 | 22 | 23 | 0 Uhr |
|---|---|---|---|---|---|---|
| 65 % | 70 % | 85 % | 80 % | 70 % | 60 % | 55 % |

## Image

Nach jeder Programmquote werden die Sender nach Quote sortiert: Platz 1 bekommt +1 Image,
Platz 3 −1. Bei Gleichstand ändert sich nichts. Ein Skandalfilm (FSK 18) vor 21 Uhr zählt dabei
als Quote 0. Mehr in [Image und Chef](Image-und-Chef.md).

---
Im Code: `ratings_calc` (src/game/seg_3264.c), `ratings_rank_update` und `newscast_quality_sum`
(src/game/seg_2bd1.c), `daily_topic_pick` (src/game/seg_3264.c); Tabellen DS:7140 und DS:1ce0
kommen aus 208.HES.

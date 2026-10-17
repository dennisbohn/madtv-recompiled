# Filme und Serien

## Aktualität

Jeder Film hat eine **maximale** und eine **aktuelle Aktualität**. Die aktuelle ist der Grundwert
für die Quote (siehe [Quoten](Quoten.md)).

- **Abnutzung:** nach jeder Ausstrahlung sinkt sie um *Maximum × Abnutzungsfaktor des Films ÷ 5*,
  höchstens bis 1.
- **Erholung:** an jedem geraden Spieltag steigt sie auf das nächste Fünftel des Maximums.

Serienfolgen funktionieren genauso, aber feiner: Abnutzung *Maximum × Faktor der Serie ÷ 20*,
Erholung in Zwanzigsteln. Außerdem teilt sich der Wert einer Serie durch die Zahl ihrer
Ausstrahlungen am selben Abend.

## Preis

**Preis = Grundpreis × (½ + ½ × aktuelle ÷ maximale Aktualität)**, auf 1.000 gerundet. Ein
abgenutzter Film kostet also bis zur Hälfte weniger. Serien: Grundpreis je fertiger Folge, mit
derselben Formel über die Summe der Folgen.

## Filmhändler

- Zehn Angebote, jeden Tag neu gemischt; nicht gekaufte gehen zurück in den Topf.
- Solange kein Sender mehr als 12 Mio. Zuschauer erreicht, sind bis zu drei billige Filme
  (höchstens 60.000) dabei.
- 77 Filme gibt es nur in der Auktion.
- Live-Übertragungen müssen zu ihrer festen Uhrzeit laufen und verfallen nach wenigen Tagen
  (das Spiel meldet einen verpassten Termin).

## Auktion

- Bis zu acht Lose. Abgerechnet wird zum Tageswechsel: das höchste Gebot gewinnt.
- Mindesterhöhung: 10 % des aktuellen Preises, abgerundet auf die erste Stelle
  (z. B. 10 % von 370.000 = 37.000 → 30.000).
- Ein Los ohne Gebot wird nach drei Tagen zurückgezogen.
- Mit schlechtem Image darf man nicht mitbieten.

## Sonderprogramme

- **NFL** und **Olympia**: Rechte mit Ablauftag, werden verlängert oder entzogen.
- **"Kultur Heute"**: jede Ausstrahlung hebt den Betty-Balken um 5 (höchstens bis zum eigenen
  Image). Wird die Sendung drei Tage nicht gesendet, kündigt B. Botterblom die Zusammenarbeit und
  sie kommt wieder in die Auktion.

---
Im Code: `film_wear`, `series_episode_wear` (src/game/seg_2a62.c), `programmes_topicality_recover`,
`film_price`, `series_price` (src/game/seg_2bd1.c), `market_update_prices`, `auction_day_end`
(src/game/seg_3264.c), `auction_bid_increment` (src/game/seg_2a62.c).

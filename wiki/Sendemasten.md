# Sendemasten

**Was man im Spiel sieht:** Preis und (neue) Zuschauer im Tooltip auf der Karte, die
Sender-Gebühren im Finanzbericht. Wie der Preis entsteht, ist intern.

## Kaufpreis

Es gibt nur vier Preise. Maßgeblich ist, wie viele Menschen **insgesamt** im Sendekreis wohnen:

| Zuschauer im Sendekreis | Kaufpreis | Kosten pro Tag |
|---|---|---|
| unter 2 Mio. | 50.000 | 10.000 |
| 2 bis unter 4 Mio. | 100.000 | 20.000 |
| 4 bis unter 10 Mio. | 250.000 | 50.000 |
| ab 10 Mio. | 500.000 | 100.000 |

Gerechnet wird Bevölkerung im Kreis ÷ 40, dann auf die nächste Stufe aufgerundet.

**Achtung:** Der Tooltip zeigt die *neuen* Zuschauer, der Preis richtet sich aber nach *allen*
Menschen im Kreis – auch denen, die man schon erreicht. Ein Mast, der stark mit vorhandenen
überlappt, ist teuer und bringt wenig.

## Tägliche Kosten

Jeden Tag ab Tag 2 werden **20 % des Kaufpreises aller eigenen Masten** fällig (Posten
"Sender-Gebühren"), auch für Satellit und Kabel, solange sie eingeschaltet sind. Ein teurer Mast
hat seinen Kaufpreis nach fünf Tagen noch einmal gekostet.

Der Startsender jedes Spielers reicht etwa 12 Mio. Zuschauer weit, ist mit rund 300.000 bewertet
und kostet damit gut 60.000 pro Tag.

## Kaufen und Verkaufen

- Ein neuer Mast wird erst **am Tagesende bezahlt**. Bis dahin lässt er sich kostenlos zurücknehmen;
  das vorgemerkte Geld ist aber schon nicht mehr verfügbar.
- Beim Setzen prüft das Spiel nur, ob das Geld für die *neuen* Zuschauer reicht (gerundet auf
  1.000). Abgebucht wird später der Stufenpreis – die Prüfung kann also durchgehen, obwohl am
  Abend mehr abgebucht wird.
- Höchstens 30 Masten (plus Satellit und Kabel). Der letzte Mast lässt sich nicht verkaufen.
- **Verkaufen** bringt nur Geld, wenn einer der Computergegner bietet; der Käufer übernimmt den
  Mast. Ohne Käufer wird er abgerissen – ohne Erlös, aber auch ohne weitere tägliche Kosten.

---
Im Code: `room_click_station_map` (src/game/seg_1581.c), `hover_station` (src/game/seg_239a.c),
`price_step_next`, `stations_reach_update`, `daily_costs_pay` (src/game/seg_2bd1.c),
`stations_settle` (src/game/seg_300f.c); Preisstufen DS:128a in src/game/ds_init.c.

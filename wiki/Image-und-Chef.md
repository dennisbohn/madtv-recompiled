# Image und Chef

## Image

Das Image (0–100) der drei Sender ergibt **zusammen immer 100** – was einer gewinnt, verlieren
die anderen.

- Nach jeder Programmquote: Platz 1 +1, Platz 3 −1, bei Gleichstand nichts.
- Ein Skandalfilm vor 21 Uhr zählt als Quote 0.
- Das Image entscheidet über Werbeverträge, Kredit, Nachrichten-Abos, die Teilnahme an der
  Auktion und darüber, ob Darsteller mitspielen.

## Ausscheiden

- Fällt das Image zum Tageswechsel unter **5**, scheidet der Sender aus; beim Menschen folgt die
  Kündigung durch den Chef.
- Scheidet ein Computergegner aus, wird sein Image verteilt.
- Der Computergegner mit dem schlechtesten Image bekommt bis Tag 39 jeden Tag 100.000 Zuschuss.

## Gespräche mit dem Chef

Die Begrüßung richtet sich nach dem Image (unter 5 gefeuert, unter 15 "Sie arbeiten ja auch noch
hier", unter 75 neutral, ab 75 freundlich). Danach kritisiert er leere Programm- und Werbeblöcke
vom Vortag und erinnert an den Sammy-Wettbewerb.

**Kündigen** wirkt sofort, wenn das Image unter 75 liegt und man heute keinen Sammy gewonnen hat.
Sonst versucht der Chef, einen umzustimmen.

---
Im Code: `ratings_rank_update`, `image_distribute` (src/game/seg_2bd1.c), `day_check_players`
(src/game/seg_3264.c), `room_click_boss` (src/game/seg_19e6.c).

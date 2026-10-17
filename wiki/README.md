# Mad TV Wiki

Spielmechanik von *Mad TV* (Release-Version), abgeleitet aus dem übersetzten Quelltext dieses
Ports. Alle Zahlen stammen aus dem Code bzw. den Startwerten der Spieldaten, nicht aus Handbuch
oder Erfahrung; am Ende jeder Seite steht, wo man es im Code nachlesen kann.

Vieles davon rechnet das Spiel nur intern und zeigt es nirgends an. Jede Seite beginnt deshalb mit
**Was man im Spiel sieht**; was als *intern* markiert ist, lässt sich im Spiel nicht einsehen –
man merkt es nur an den Folgen (Quoten, Preise, Antworten).

- [Quoten](Quoten.md) – wie Zuschauerzahlen entstehen, Genre und Uhrzeit, Thema des Tages
- [Filme und Serien](Filme-und-Serien.md) – Preise, Aktualität und Abnutzung, Filmhändler, Auktion
- [Nachrichten](Nachrichten.md) – Wert, Preis, Alterung, Abos
- [Werbung](Werbung.md) – Verträge, Mindestzuschauer, Prämie und Strafe
- [Sendemasten](Sendemasten.md) – Preisstufen, tägliche Kosten, Verkauf
- [Finanzen](Finanzen.md) – Startkapital, tägliche Kosten, Kredit, Gerichtsvollzieher
- [Image und Chef](Image-und-Chef.md) – Image, Rauswurf, Gespräche mit dem Chef

## Begriffe

- **Stunde / Block:** gesendet wird von 18 bis 1 Uhr, also sieben Blöcke (18, 19, … 0 Uhr).
  Zu jeder vollen Stunde laufen Nachrichten, ab Minute 5 das Programm, um Minute 55 die Werbung.
- **Reichweite:** alle Menschen, die die eigenen Sender erreichen. Die Karte rechnet in
  Bevölkerungseinheiten, eine Einheit sind 2464 Zuschauer.
- **Quote:** Anteil der eigenen Reichweite, der zuschaut (in Prozent). Zuschauer = Reichweite ×
  Quote.
- **Genres:** 1 Lovestory, 2 Actionfilm, 3 Monumentalfilm, 4 Komödie, 5 Krimi, 6 Kultur,
  7 Live-Übertragung, 8 Show, 9 Science Fiction, 10 Sonstiges; Serien: 11 Familienserie,
  12 SF-Serie, 13 Western-Serie, 14 Krimi-Serie, 15 Sonst. Serie.

# Horizon Transit: Machbarkeit und Reihenfolge der Erweiterungen

Stand: 16.11.2026, Version 0.4.0. Grundlage sind 10 Ideen aus der Community vom 16.11.2026.

## Was die Mod heute schon weiß (worauf alles aufbaut)

| Daten | Quelle | Im Spiel bestätigt? |
|---|---|---|
| Position, Tempo und Fahrstil des eigenen Autos | Hook auf das Auto-Update des Spiels | ja (Log, Fahrten mit Bezahlung) |
| Welches Auto gefahren wird (zum Beispiel `DOD_ChargerFast7_15`) | Name im Auto, Stelle +0x350 | ja (Log 16.11. 19:07) |
| Marke, Modell, Preis und Klasse aller 353 Autos | Spieldatenbank `gamedbRC.slt`, Tabelle `Data_Car` | Daten ja, gelesen beim Bauen |
| **Fahrzeuggruppe** (Supersportler, Limousine, Klassiker, Geländewagen …) | `Data_Car.FamilyBodyID`, das sind die 30 Gruppen des Autohauses | Daten ja, noch nicht genutzt |
| Cabrio oder offen | `Data_Car.IsOpenCockpit` (19 Autos) | Daten ja, noch nicht genutzt |
| 208 echte Orte | Trigger-Zonen des Spiels | ja (GPS führt hin) |
| GPS-Ziel setzen | Navigationsfunktion des Spiels | ja |
| Pause, Karte, Menü offen? | Plattform (`FH2_UI_ON_SCREEN`) | ja (Handy-Fenster wartet richtig) |
| Echte Uhrzeit des PCs | Windows | ja |
| **Uhrzeit und Wetter im Spiel** | nicht bekannt | **nein**, siehe Idee 5 |
| Credits des Spielstands | Gutschrift-Funktion des Spiels `game+0x9E9120` | **ja, seit 0.13** (alles Geld der Mod; im Spiel ungetestet) |

## Machbarkeit der 10 Ideen

| # | Idee | Machbar? | Wie | Aufwand | Grenzen |
|---|---|---|---|---|---|
| 1 | Fahrgastgedächtnis | **ja**, zum Teil schon da | 40 Personen erzählen ihre Geschichte weiter, Stammgäste grüßen anders. Neu: pro Person das letzte Auto und die letzte Bewertung merken („Heute nicht im Miura?“, „Letztes Mal war's ruppig“). | klein | etwa 4 Byte pro Person in der INI |
| 2 | Fahrzeugklassen und Vorlieben | **ja** | Gruppe aus `FamilyBodyID` beim Bauen in die Autoliste übernehmen. Jede Person bekommt Vorlieben (mag Limousinen, hasst Geländewagen …), das wirkt auf Sprüche, Trinkgeld und Zufriedenheit. | mittel | Car-Pack-Autos haben keine Gruppe und gelten als „normal“. Sitzplätze kennt die Datenbank nicht. |
| 3 | Busfahrplan | **ja** | Fahrplan pro Linie aus den Abständen der Haltestellen. Ankunft, Verspätung, Haltezeit und Pünktlichkeit pro Halt. Anzeige im Handy. | mittel | Die Zeiten laufen in echter Zeit ab Linienstart, nicht in Spielzeit (siehe 5). |
| 4 | Unternehmensruf | **ja** | Ein Wert von 0 bis 100 aus den letzten Bewertungen. Er bestimmt, wie viele Anfragen kommen, wie gut sie bezahlt sind und ab wann VIPs kommen. | klein | – |
| 5 | Dynamische Nachfrage | **teilweise** | **Ort:** ja, mehr Anfragen in Nizza und am Festival, weniger auf dem Land. **PC-Uhrzeit:** ja, möglich, aber die passt nicht zur Sonne im Spiel. **Spiel-Uhrzeit und Wetter:** unklar. Extra Options kann sie einstellen, aber eine Stelle zum Auslesen kenne ich noch nicht. Die müsste ich erst im Spiel suchen. | Ort klein, Spielzeit unbekannt | Ohne gefundene Spielzeit kein „nachts mehr Partygäste“. |
| 6 | Fahrzeugflotte | **ja, mit Grenze** | Auftragsarten passend zur Fahrzeuggruppe: Limousine für Geschäftskunden, Geländewagen für Bergtouren, Sportwagen für Fans, Bus nur für Linien (schon so). | mittel | Die Mod sieht nur das **gerade gefahrene** Auto, nicht deine Garage. „Flotte“ heißt also: welches Auto du für welchen Auftrag nimmst. |
| 7 | Besondere Interaktionen | **ja, als Gespräch** | Wünsche, die die Mod prüfen kann: „langsamer bitte“, „ruhiger in den Kurven“, „kurz am Bäcker halten“ (Zwischenstopp mit GPS), „zeig mal, was er kann“. Erfüllt der Fahrer den Wunsch, gibt's Pluspunkte. | mittel | **Musik und Fenster** steuert die Mod nicht. Solche Wünsche gibt es nur als Spruch, ohne Prüfung. Beim Cabrio („Dach auf?“) kennt die Mod nur das Modell. |
| 8 | VIP- und Sonderaufträge | **ja** | Ab einem bestimmten Ruf: VIP mit Wunschauto-Gruppe, engem Zeitlimit, Höchsttempo und strenger Bewertung. Dafür zahlt er das Doppelte oder mehr. Sonderaufträge: Flughafenfahrt mit Gepäck, Hochzeitspaar, Nachtschwärmer. | mittel | baut auf 2 und 4 auf |
| 9 | Langfristige Statistiken | **ja**, zum Teil schon da | Schon gespeichert: Kilometer, Fahrten, Buslinien, Fahrgäste, Einnahmen, Bewertung. Neu: Trinkgeld, Unfälle, Pünktlichkeit, Bestwerte, pro Linie, pro Person. Im Handy als Tab „Konto“. | klein | – |
| 10 | Arbeitstag | **ja** | „Schicht beginnen“ und „Feierabend“ im Handy. Die Abrechnung zeigt Fahrten, Einnahmen, Trinkgeld, Kilometer, Durchschnitt und die beste Fahrt, dazu die Sterne des Tages. | klein bis mittel | – |

## Reihenfolge

Jede Stufe ist für sich spielbar und getestet, bevor die nächste beginnt. Was schon läuft, bleibt, wie es ist.

**Stufe 1: Grundlagen (klein, kein Risiko)**
1. **Statistiken (9):** Alle weiteren Ideen brauchen diese Zahlen.
2. **Ruf (4):** eine Zahl aus den Statistiken, sofort spürbar.
3. **Gedächtnis erweitern (1):** letztes Auto und letzte Bewertung pro Person.

**Stufe 2: Berufsalltag**

4. **Arbeitstag (10):** nutzt die Statistiken für die Abrechnung.
5. **Busfahrplan (3):** eigenes Modul für den Busmodus.

**Stufe 3: Autos zählen**

6. **Fahrzeugklassen und Vorlieben (2):** Gruppe aus der Datenbank.
7. **Flotte (6):** Auftragsarten passend zur Gruppe.
8. **VIP und Sonderaufträge (8):** braucht Ruf und Klassen.

**Stufe 4: Leben in der Welt**

9. **Interaktionen (7):** Wünsche mit Prüfung, Zwischenstopps.
10. **Nachfrage (5):** zuerst nach Ort. Spielzeit und Wetter kommen nur, wenn ich eine sichere Stelle im Spiel finde.

## Regeln für den Einbau

- **Modular:** Jede Idee kommt in eine eigene Datei unter `src/core/` (zum Beispiel `reputation.c`, `shift.c`, `timetable.c`), reines C ohne Spielaufrufe, mit eigenen Tests.
- **Performance:** keine Arbeit pro Bild. Die Logik läuft 10-mal pro Sekunde wie bisher. Gespeichert wird nur am Ende eines Auftrags. Alle neuen Daten zusammen sind unter 10 KB.
- **Keine Wiederholungen:** Sprüche nie zweimal hintereinander, Geschichten gehen der Reihe nach weiter, neue Themen bekommen eigene Spruchlisten.
- **Abschaltbar:** Neue Systeme mit Einfluss auf Geld oder Schwierigkeit bekommen einen Schalter im Mod-Menü.
- **Ehrlich:** Was die Mod nicht prüfen kann (Musik, Fenster, Spielzeit), wird nicht als echte Spielfunktion dargestellt.

## Stand 0.3 (bereits gebaut, im Spiel noch ungetestet)

- 6 Buslinien statt 3. Die Strecken sind fest, die Fahrgastzahlen an jedem Halt zufällig.
- 40 feste Personen mit Alter, Beruf und je 6 Sätzen aus ihrem Leben. Stammgäste erkennen dich wieder, ihre Geschichte geht beim nächsten Mal weiter. Gespeichert in `TransportSim.ini` unter `[Stats] Met`.
- Rund 400 allgemeine Sprüche (vorher 200) plus 240 persönliche, zusammen etwa 640.
- Im Bus spricht jeweils eine Person mit Namen statt „Fahrgäste“.
- 4851 automatische Prüfungen bestanden.

## Stand 0.4.0 (gebaut, im Spiel noch ungetestet)

Alle 4 Stufen sind eingebaut, ohne die beiden Teile, die nicht gehen (Spiel-Uhrzeit/Wetter, Musik/Fenster).

| # | Idee | Stand |
|---|---|---|
| 9 | Statistiken | eingebaut, Konto-Tab und `[Career]` in der INI |
| 4 | Ruf | eingebaut (`career.c`) |
| 1 | Gedächtnis | eingebaut: letztes Auto und letzte Sterne pro Person |
| 10 | Arbeitstag | eingebaut: Schicht und Abrechnung (nicht über einen Neustart gespeichert) |
| 3 | Busfahrplan | eingebaut (`timetable.c`) |
| 2 | Klassen und Vorlieben | eingebaut: 8 Gruppen aus `FamilyBodyID`, Vorlieben pro Person |
| 6 | Flotte | eingebaut als Auftragsarten nach dem gefahrenen Auto |
| 8 | VIP und Sonderaufträge | eingebaut: VIP, Geschäftskunde, Hochzeitspaar, Ausflug |
| 7 | Interaktionen | eingebaut: 4 prüfbare Wünsche (`wishes.c`); Musik und Fenster weggelassen |
| 5 | Nachfrage | eingebaut nach Ort; Tageszeit und Wetter weggelassen |

Abschaltbar: „Ruf, Sonderfahrten, Wünsche“ im Mod-Menü. 5734 automatische Prüfungen bestanden.

## Stand 0.5.0: auf dem Weg zu 1.0 (gebaut, im Spiel noch ungetestet)

25 Ideen gesammelt (Marvins Wünsche, Spielerwünsche aus dem Internet, eigene), jede gegen die Machbarkeit geprüft. 15 sind eingebaut, 10 gehen nicht oder würden die Mod überladen. Die ganze Liste mit Herkunft und Begründung: [IDEAS-1.0.md](IDEAS-1.0.md).

| Bereich | Eingebaut in 0.5.0 |
|---|---|
| Geräte | Edge-Handy (Samsung-artig), Tablet mit zweiter Spalte (Fahrgast-Profil, Streckenplan, Fahrtenbuch) |
| Bus | Linien in beide Richtungen, Haltestellenansagen (auch gesprochen), Haltewunsch mit Durchfahren, Fahrgeld-Kasse am Tacho |
| Fahrdienst | Push-Anfragen während der Schicht, Stammgäste fragen nach dir, Fundsachen, Taxameter am Tacho |
| Karriere | Serie (Bonus für gute Aufträge hintereinander), Tagesziel pro Schicht |

**Für 1.0 noch offen:** alles von 0.4 und 0.5 im Spiel testen und die Zahlen danach einstellen (wie oft Push-Anfragen kommen, Fundsachen, Bonushöhen). Ob der Bus Tür-Animationen hat, lässt sich nur im Spiel prüfen.

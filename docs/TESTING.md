# Testanleitung: Horizon Transit 0.4.0 (Prototyp)

Dauer: etwa 20–30 Minuten. Du brauchst die FH2 Mod Platform 2.9.0, die du schon installiert hast.

## 1. Installieren

1. **Vorher sichern (empfohlen):** Kopiere deinen Spielstand-Ordner an einen sicheren Ort. Die Mod fasst den Spielstand nicht an. Nur der experimentelle Bus-Schalter ändert die Spieldaten, und auch der nur, wenn du ihn selbst einschaltest.
2. Kopiere `TransportSim.dll` nach `C:\Users\Flenkii\Documents\Azimuth\fh2-mods\`. Es ist eine neue Datei, keine bestehende wird ersetzt.
3. Starte das Spiel **offline**. Online schaltet sich die Mod selbst ab.

**Deinstallieren:**

- Lösche `TransportSim.dll`.
- Den Ordner `Azimuth\TransportSim\` (Einstellungen, Konto, Logs) kannst du ebenfalls löschen.

## 2. Bedienung

| Taste | Controller | Funktion |
|---|---|---|
| F9 | LB+RB+Steuerkreuz hoch | Handy öffnen oder schließen (öffnen setzt bei laufendem Auftrag das GPS neu) |
| F10 | LB+RB+Steuerkreuz rechts | Nächster Eintrag im Handy |
| F11 | LB+RB+A | Annehmen. Während eines Auftrags: zweimal drücken = abbrechen |
| Pfeil hoch / runter | – | Nur bei offenem Handy: Eintrag wählen |
| Enter | – | Nur bei offenem Handy: annehmen (während eines Auftrags zweimal = abbrechen) |
| Rücktaste | – | Nur bei offenem Handy: Handy schließen |

Solange das Handy offen ist, wirken Pfeiltasten, Enter und Rücktaste **nur** im Handy, nicht im Spiel, nicht in Gripline und nicht in FH2 Mouse. (Seit 0.4.1 immer so, ohne Schalter.)

- **Das Gleiche ohne Tasten:** Pause › MODS › YOUR MODS › Horizon Transit › Horizon Transit.
- **Einstellungen** findest du unter Pause › MODS › YOUR MODS › Horizon Transit: Auto-GPS, Zeitlimits, Sprache, Handy-Design, „Ruf, Sonderfahrten, Wünsche“ und Bus im Autohaus.
- **Hinweis zum Controller:** LB ist bei NitroSystem der Nitro-Knopf. Die Kombination kann daher kurz Nitro zünden. Wenn dich das stört, belege die Tasten im Mod-Menü neu.

## 3. Was du testen sollst, in dieser Reihenfolge

### Test A: Lädt die Mod?

1. Starte das Spiel und fahr ein paar Sekunden frei herum.
2. Öffne Pause › MODS › YOUR MODS. Steht dort „Horizon Transit“?
3. Öffne die neueste Log-Datei in `Azimuth\TransportSim\logs\` und suche diese Zeilen:
   - `car update hooked`
   - `sat-nav hooked`
   - `HUD layer added`
   - `the own car is seen`

   Steht dort irgendwo **NOT**, schick mir das Log.

### Test B: Eine Uber-Fahrt

1. Drück **F9**. Links sollte das Handy erscheinen, mit 3 Fahraufträgen (Ziel, km, Bezahlung) und 3 Buslinien.
2. Wähl mit **F10** eine Fahrt aus und nimm sie mit **F11** an.
   - Erscheint ein Pop-up „Fahrt angenommen“?
   - **Zeigt das GPS auf der Minimap eine Route zum Abholort?** Das ist der wichtigste Punkt.
   - Siehst du oben Status und Entfernung, und oben rechts Zeit und Zufriedenheit mit Sternen?
3. Fahr zum Abholort und halte dort an (unter 7 km/h, höchstens 30 m vom Ziel entfernt).
   - Läuft ein 3-Sekunden-Ring ab?
   - Erscheint danach „Fahrgast eingestiegen“?
   - Springt das GPS auf das Fahrziel um?
4. Fahr jetzt zum Ziel. Fahr einmal ruhig und einmal absichtlich ruppig: Vollgas, Vollbremsung, gegen eine Wand.
   - Sinkt die Zufriedenheit oben rechts?
   - Kommen Chat-Sprüche des Fahrgasts über der Minimap?
5. Halte am Ziel an. Nach 2 Sekunden sollte ein Pop-up mit der Bezahlung, den Sternen und der Zusammenfassung kommen.

### Test B2: Neu in 0.2.0

1. Steig in einen Lamborghini oder Ferrari und öffne das Handy. Steht oben dein Auto beim Namen? Gibt es unter den Fahrten öfter „Auto-Fan“?
2. Während einer Fahrt: Steht vor dem Einsteigen oben „Max N“ in derselben Einheit wie der Tacho (Spiel auf Englisch: mph)? Sobald der Fahrgast sitzt: Ist oben links das runde Tempolimit-Schild zu sehen? Fahr schneller. Blinkt das Schild gelb, kommt „Zu schnell!“, und sinkt die Zufriedenheit?
3. Pausenmenü > MODS > YOUR MODS > Horizon Transit: Sind alle Einstellungen und Beschreibungen in der Sprache des Spiels?
4. Bei einem Auto-Fan: Fahr mal langsam. Drängelt er nach etwa 15 s? Gib Gas. Freut er sich?
5. Wechsle im Handy mit Pfeil links/rechts zwischen den Tabs Fahrten, Bus und Konto.
6. Wähl mit einem normalen Auto eine Buslinie. Kommt „Dafür brauchst du den Bus“?

### Test B3: Neu in 0.4.0

1. **Konto-Tab:** Steht oben „Schicht beginnen“? Drück Enter. Kommt „Schicht beginnt“? Darunter: Ruf 50 · Gut.
2. **Fahrzeuggruppe:** Fahr mit einer Limousine (zum Beispiel Audi RS5 oder BMW M5) und öffne das Handy. Kommen Geschäftskunden? Mit einem Klassiker ein Hochzeitspaar (ab Ruf 45)? Mit einem Geländewagen ein Ausflug (lange Fahrt)?
3. **Gegend:** Am Festival oder in Nizza sollte über den Fahrten „Viel los“ stehen, auf dem Land „Ruhige Gegend“ mit nur 2 Anfragen.
4. **Wunsch:** Nimm eine lange Fahrt. Nach etwa 35 bis 75 Sekunden kann ein Wunsch kommen. Erscheint oben ein vierter Chip („Wunsch: unter 60 km/h · 0:28“)? Bei „kurz halten“: Springt das GPS auf einen anderen Ort und danach zurück zum Ziel?
5. **Gedächtnis:** Fahr dieselbe Person ein zweites Mal (Name in der Anfrage). Erwähnt sie das Auto vom letzten Mal?
6. **Busfahrplan:** Steht während der Linie oben „Fahrplan: in m:ss“? Kommt am Ende „Pünktlich an x von y Halten“?
7. **Feierabend:** Konto-Tab, Enter. Kommt „Feierabend · N CR“ mit der Abrechnung?
8. **Ruf nach Fahrten:** Nach einer 5-Sterne-Fahrt sollte im Chat „Ruf +5.0“ stehen.
9. **Schalter:** Im Mod-Menü „Ruf, Sonderfahrten, Wünsche“ ausschalten. Dann sollte es keine Wünsche und keine Sonderfahrten mehr geben.

### Test C: Eine Buslinie (nur mit dem Bus, siehe Test E; oder schalte „Buslinien nur im Bus“ im Mod-Menü aus)

1. Drück F9, wechsle zum Tab „Bus“, wähl „Linie 1 · Festival-Ring“ und nimm sie an.
2. Das GPS sollte zur ersten Haltestelle zeigen, dem Horizon Festival.
3. Halte an jeder Haltestelle an. Dann sollten diese Meldungen kommen:
   - „N Fahrgäste steigen aus“
   - „N Fahrgäste steigen ein“

   Danach springt das GPS zur nächsten Haltestelle.
4. Probier einmal, zu früh loszufahren. Die restlichen Fahrgäste bleiben dann stehen, und es gibt einen Abzug.
5. Prüf die Plausibilität: Es dürfen nie mehr Leute aussteigen, als drin sind. An der Endhaltestelle steigen alle aus.

### Test D: Abbrechen und Störungen

- Drück während eines Auftrags F9 und dann zweimal F11. Der Auftrag sollte abgebrochen sein, und das GPS-Ziel der Mod verschwindet.
- Drück während eines Auftrags die Pause. Die Zeit sollte stehen bleiben.
- Spul zurück (Rewind). Das sollte **nicht** als Unfall zählen.

### Test E (nur wenn du magst, EXPERIMENTELL): Der echte Bus

1. Schalte im Mod-Menü „Bus im Autohaus (experimentell)“ ein (bei dir schon erledigt).
2. Starte das Spiel wie immer, **auch über den FH2MP-Launcher** (seit 0.4.0). Vorher legt die Mod ein Backup deines Spielstands an. Bleib dabei offline, geh nicht in den Online-Freeroam.
3. Sieh im Autohaus nach (in der Gruppe des Ford Transit SSV): Ist dort ein Bus?
4. Wenn ja, kaufen und fahren. Achte auf Kamera, Cockpit und Fahrer.
5. Wenn etwas kaputt ist oder das Spiel abstürzt: Schalte den Schalter aus, oder schreib in `Azimuth\TransportSim\TransportSim.ini` `BusInAutoshow=0`, und starte neu.

### Test F (0.4.3, Versuch): Fahrgast-Türen

1. Nur `fh2-mods\TransportSim.dll` ersetzen (die Türgeräusche stecken darin).
2. Pause > MODS > Horizon Transit: „Fahrgast-Türen“ ist standardmäßig an.
3. Mit einem Viertürer (z. B. Jeep Grand Wagoneer) eine Uber-Fahrt annehmen und zum Fahrgast fahren. Stehen bleiben.
4. Auf die Außenkamera schalten: Geht eine Tür auf und wieder zu? Hörst du das Geräusch zur richtigen Zeit?
5. Am Ziel: Geht dieselbe Tür auf und zu?
6. Einmal mit offener Tür losfahren: Geht die Tür sofort zu?
7. Rechtslenker: Ein Auto mit Lenkrad rechts nehmen, kurz in der Cockpit- oder Fahrerkamera fahren (das Log schreibt „the wheel is on the RIGHT“), dann eine Fahrt: Die vordere Tür muss links aufgehen.
8. Danach bitte das Log aus `Azimuth\TransportSim\logs` schicken. Wichtig sind die Zeilen mit `doors:`.

Passt das Geräusch zeitlich nicht, kann man `ShutDelayMs` in `TransportSim.ini` ändern (kleiner = früher).

### Test G (0.5.0): Edge, Tablet und die neuen Kleinigkeiten

1. Handy öffnen (F9), Tab **Optionen**, ganz oben **Gerät**: Enter schaltet Handy → Edge → Tablet. Sieht jedes gut aus? Passt das Tablet auf deinen Bildschirm, ohne die Minikarte zu verdecken?
2. Auf dem Tablet durch die Tabs gehen: Fahrten (rechts das Profil des Fahrgasts), Bus (Streckenplan), Konto (Fahrtenbuch).
3. **Konto > Schicht beginnen**, dann ohne Auftrag herumfahren: Nach etwa einer Minute kommt „Neue Anfrage“, danach alle 2 bis 3 Minuten. F9 zeigt diese Anfrage oben.
4. Eine Fahrt annehmen: Unten rechts in der Fahrt-Anzeige läuft der **Taxameter** mit.
5. Eine Buslinie fahren: Die **Ansage** „Nächster Halt“ kommt beim Losfahren. Bei „Kein Haltewunsch“ einfach durchfahren. Mit „Stimmen“ an wird die Ansage vorgelesen.
6. Eine Linie vom anderen Ende aus starten: Sie fährt rückwärts.
7. Rückmeldung: Kommen Push-Anfragen zu oft oder zu selten? Ist das Tablet zu groß?

## 4. Was ich als Rückmeldung brauche

- Das Log aus `Azimuth\TransportSim\logs\`.
- Für jeden Test kurz: hat geklappt, hat nicht geklappt, oder sah komisch aus. Ein Screenshot vom HUD und vom Handy hilft sehr.
- Wie sich die Zufriedenheit anfühlt: zu streng, zu locker oder passend.


## 0.5.1: Geräusche, Wünsche, Antworten

1. Fahrgast einsteigen lassen, langsam über das Limit fahren: Wird das Schild gelb, kommt einmal ein leiser Ton? Bleib gelb: kommt er nicht noch mal? Kurz langsamer (2 s), dann wieder gelb: kommt er wieder?
2. Deutlich zu schnell (Schild rot): läutet es jede Sekunde „Ding-Ding“? Langsamer bis gelb: hört es sofort auf?
3. Mod-Menü > Mod-Geräusche auf Leise / Laut / Aus: ändert sich die Lautstärke (beim Umstellen kommt ein Probe-Ping)? Bei Aus: wieder die Menü-Geräusche des Spiels.
4. Buslinie: Gong vor jeder Ansage? Glocke ein paar Sekunden später, wenn jemand aussteigen will?
5. Schicht starten und herumfahren: Ping bei der Push-Anfrage? Nach einer Fahrt: Kassen-Klang statt des normalen Pop-up-Tons?
6. Wunsch „unter 70“: zeigt das Schild 70 bis zum Ziel? Steht unten „Bis Ziel: unter 70“ statt einer Uhr?
7. Im Supersportwagen (z. B. Lamborghini Huracán) ein paar längere Fahrten: kommt mal „Gib Vollgas“? Zeigt das Schild dann den weißen Kreis mit schwarzen Streifen? Kommt der Jubel bei 180 km/h (110 mph)?
8. Mit Tastatur fahren: kommt mal eine Frage mit „[B] Antworten“ unten? B drücken: steht „Du: …“ im Feed, mit „Stimmen“ an mit tiefer Männerstimme? Reagiert der Fahrgast? Nur mit Controller fahren (2 Minuten keine Tastatur): kommen keine Fragen mehr?

## 0.6.0: Karten, Fahrtarten, Hotspots, Stufen, Sammelfahrten

1. **Abschluss-Karte:** Eine Fahrt beenden. Unten rechts kommt die Karte: Sterne leuchten nacheinander, das Geld zählt hoch, ein Kommentar, unten Fahrpreis/Trinkgeld/Ankunft. Nach 8 Sekunden gleitet sie weg. Kommt KEIN Bezahl-Pop-up mehr? (Der Kassen-Klang soll bleiben.)
2. **Schicht-Rückblick:** Konto > Schicht beginnen, 2 Fahrten, dann Feierabend machen. Kommt die Karte mit Verdienst, Fahrten, Sternen, Strecke, Trinkgeld und Medaille?
3. **Animationen:** Gleitet die Fahrt-Karte beim Annehmen von unten rein? Füllt sich die Route weich? Gleitet das Handy beim Öffnen von links rein? Ruckelt dabei etwas im Spiel?
4. **Fahrtarten:** Mit einem Kleinwagen nur „Spar“-Anfragen, mit einem Geländewagen auch „XL · Name +2“, mit Luxuslimousine/Supersportwagen ab Silber auch „Black“.
5. **Hotspots:** Nach etwa einer Minute steht oben in der Fahrten-Liste ein Ort mit „1,3x“ o. Ä. Hinfahren: Anfragen von dort zeigen den Aufpreis, die Fahrt-Karte ein gelbes „1,3x“ neben dem Taxameter.
6. **Stufen:** Konto > „Fahrer-Stufe“ mit Balken. Wenn du eine Stufe erreichst, kommt nach der Abschluss-Karte eine Karte mit Medaille.
7. **Sammelfahrt:** Eine Anfrage „Sammelfahrt · A & B“ annehmen. Erst A abholen, dann B (die Karte sagt „2. FAHRGAST ABHOLEN“), dann steigt A aus und zahlt, dann B ans Ziel. Am Ende eine Karte mit beiden Namen.

## 0.7.0: Die Karte im Handy

1. `Karte.bmp` liegt in `Azimuth\TransportSim\` (neben `TransportSim.ini`). Im Log steht `map: Karte.bmp 4096x1926`.
2. Handy öffnen, mit → zum Tab **Karte**. Stimmt der blaue Punkt mit deinem Ort im Spiel überein? Zeigt der kleine Kegel in deine Fahrtrichtung? Enter: Nah, Umgebung, ganze Karte.
3. Tab **Fahrten**, eine Fahrt wählen, Enter: Die Vorschau zeigt dich, den Abholort (grün) und das Ziel (rot). Folgen die Linien grob den Straßen? ↓ auf „Zurück“, Enter: wieder die Liste. Rücktaste in der Vorschau: auch zurück. ←→: Strecke / ganze Karte.
4. Nochmal Enter auf eine Fahrt, dann Enter auf „Fahrt annehmen“: die Fahrt startet wie gewohnt.
5. Während der Fahrt Handy öffnen, →: Tab **Karte** mit dem Weg zum Ziel. Fährt der blaue Punkt mit? Enter zoomt (bricht nichts ab). Rücktaste oder ←: zurück zum Auftrag.
6. Im Bus: Tab **Bus**, eine Linie, Enter: alle Haltestellen nummeriert. Linie starten, dann die Karte im Auftrag: erledigte Halte grau, der nächste blau.
7. Mod-Menü „Karten-Vorschau vor dem Annehmen“ aus: Enter nimmt sofort an.
8. Ruckelt das Handy beim ersten Öffnen einer Vorschau kurz? (Der Weg wird einmal berechnet, ca. 0,1 s.)

## 0.9.1: Navi und Animationen

1. Handy öffnen, Tab **Karte**, mehrmals Enter: Nah, Umgebung, Ganze Karte, **Navi**. Jeder Wechsel soll weich gleiten, nichts darf springen.
2. In **Navi** losfahren: Fahrtrichtung zeigt nach oben, du bist im unteren Drittel, die Karte dreht sich ruhig mit. Vergleiche mit dem Navi des Spiels unten links. Der Kompass oben rechts zeigt Norden.
3. Eine Fahrt annehmen, im Auftrag nach rechts auf **Karte**, Enter bis **Navi**: der Weg zum Ziel soll vor dem Pfeil liegen.
4. Tabs wechseln, in Listen hoch und runter: alles blendet weich über, die Tab-Markierung gleitet.
5. Handy schließen: es blendet nach links aus.
6. Rückmeldung: Ruckelt etwas, ist etwas zu langsam oder zu schnell? Fällt die Bildrate im Spiel, wenn die Karte offen ist?

## 0.9.0: Karte aus dem Spiel

1. `Karte.bmp` aus `Azimuth\TransportSim\` löschen (oder liegen lassen, sie wird nicht mehr benutzt). Spiel starten. Im Log steht `map: from the game's files: picture ok, roads ok (... lines), 3584x2048, ... ms`.
2. Handy, Tab **Karte**: dunkle Karte mit Straßen (Autobahnen gelblich). Stimmt der blaue Punkt mit deinem Ort überein, auch nah gezoomt an einer Kreuzung in Nizza?
3. Eine Fahrt in der Vorschau: Folgen die Linien den Straßen?
4. Ruckelt das Spiel kurz nach dem Start (die Karte wird in den ersten 1 bis 2 Sekunden gebaut)?

## 0.13.0: Echte Credits

1. Merk dir deine Credits im Spiel. Das Handy (F9) zeigt oben rechts das Spielkonto, das sollte gleich sein.
2. Eine Fahrt fertig fahren. Erwartet: Pop-up „Credits gutgeschrieben, +X CR“ und die Credits im Spiel steigen um X.
3. Im Shop etwas kaufen. Erwartet: Die Credits sinken um den Preis, das Teil ist gekauft.
4. Spiel neu starten: Stimmen die Credits noch?
5. Im Log (`TransportSim\logs`) nach `credits:` suchen. „are as expected“ heißt, die Spielfunktionen passen.
6. Wenn etwas schiefgeht: Spiel zu, Sicherung aus `fh2-mods\save-snapshots\` in einen neuen, leeren saves-Ordner kopieren.

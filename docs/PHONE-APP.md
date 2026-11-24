# Handy-App (0.11.1)

Neu in 0.10.4: Die App geht ohne Spiel. Sie zeigt den letzten Stand, Optionen lassen sich ändern und gehen beim nächsten Verbinden ins Spiel (`a=set`), Kartenstücke bleiben auf dem Handy (die App fängt `/api/tile` ab und speichert WebP in ihren Dateien; „Karte speichern“ holt Stufe 0 bis 5).

Neu in 0.10.3: Querformat und Tablets. Quer steht die Tab-Leiste links; ab 700 px Breite laufen die Listen in Spalten, und ein Auftrag zeigt links die große Karte und rechts den Auftrag.

Neu in 0.10.2: Während eines Auftrags zeigt der Tab „Auftrag“ oben die Karte. Oben rechts in jeder Karte wählst du „Karte“ (Norden oben) oder „Navi“ (dreht mit dem Auto, Kompass). Die Wahl merkt sich die App.

Neu in 0.10.1: im Spiel-Handy einschalten (F9 > Optionen > „Handy-App verbinden“, dann steht dort der Code), das Design des Spiel-Handys gilt sofort auch in der App, alle Optionen des Spiel-Handys in der App (`/api/do?a=setting&n=<Zeile>`), Shop mit 9 Sachen (`a=buy`), Konto mit Statistik. Neue Felder in `/api/state`: `theme`, `settings`, `shop`, `account`. Updates der APK installieren sich über die alte (gleicher Schlüssel `android/release.p12`, höherer versionCode).



Horizon Transit läuft zusätzlich als Android-App auf deinem echten Handy. Die App zeigt live, was im Spiel passiert,
und schickt Eingaben zurück: Fahrt annehmen, Buslinie starten, Fahrgästen antworten, Auftrag abbrechen, Schicht an
und aus.

*English: An Android app that shows rides, the map, bus lines and the passengers' chat live from the game and sends
taps back. The mod runs a small web server in the home network (off by default, switch "Phone app (Wi-Fi)").*

## So funktioniert es

```
Handy (App)  ── WLAN ──>  PC: Forza Horizon 2 + Horizon Transit
                          kleiner Webserver in der Mod, Port 8710 (TCP)
                          Suche nach dem PC: Port 8711 (UDP)
```

- Die Mod startet einen kleinen Webserver im Spiel, nur wenn der Schalter **Handy-App (WLAN)** im Mod-Menü an ist.
- Die App sucht den PC selbst: Sie ruft „HORIZON-TRANSIT?“ ins WLAN, die Mod antwortet mit Name und Port. Klappt das
  nicht (manche Router blocken das), tippst du die Adresse ein. Sie steht im Mod-Menü bei der Handy-App.
- **Kopplungscode:** 4 Ziffern, beim ersten Start zufällig gewählt, steht im Mod-Menü und in
  `TransportSim.ini` unter `[PhoneApp] Code`. Ohne den Code bekommt niemand im WLAN Daten oder kann etwas auslösen.
- **Die Karte** kommt von deinem PC: Die Mod zeichnet sie aus deinen Spieldateien (wie im Handy im Spiel) und
  schickt sie in 256-Pixel-Stücken ans Handy. In der App steckt nichts aus dem Spiel.
- **Windows-Firewall:** Beim ersten Einschalten fragt Windows, ob das Spiel ins Netzwerk darf. Für **private
  Netzwerke** erlauben. Wurde das abgelehnt: Windows-Sicherheit > Firewall > „App durch die Firewall zulassen“ >
  `forza_durango_release_final.exe` bei „Privat“ anhaken.

## Einrichten

1. `HorizonTransit.apk` aufs Handy kopieren (oder per Mail/Cloud schicken) und antippen. Android fragt, ob die Quelle
   Apps installieren darf: einmal erlauben. Play Protect warnt eventuell vor einer unbekannten App: „Trotzdem
   installieren“.
2. Spiel starten, Pause > MODS > Horizon Transit > **Handy-App (WLAN)** einschalten. Ein Pop-up zeigt Adresse und Code.
3. App öffnen. Sie findet den PC, du gibst den Code ein, fertig. Beim nächsten Mal verbindet sie sich von selbst.

Ohne App geht es auch im Browser des Handys (auch iPhone): `http://<Adresse>:8710` öffnen, Code eingeben.

## Was die App kann

| Tab | Inhalt |
| --- | --- |
| Fahrten | Die Anfragen mit Bezahlung, Strecke, Tempolimit, Fahrtart, Abhol- und Zielort. „Fahrt annehmen“ startet die Fahrt im Spiel. Während eines Auftrags: Ziel, Entfernung, Tempo, Tempolimit-Schild, Stimmung, Restzeit, Wunsch, Taxameter, Antworten-Knopf, Chat, Abbrechen (zweimal tippen). |
| Karte | Umschalter „Karte“ / „Navi“ oben rechts (Navi dreht mit dem Auto, Kompass links oben; auch oben im Auftrag). Kartenstücke vom PC, Wischen und Zoomen mit zwei Fingern, dein Auto als Pfeil, der Weg zum Ziel entlang der Straßen, Abhol- und Zielort, Haltestellen der Linie, Hotspots. |
| Bus | Alle Linien mit Start- und Endhaltestelle, „Linie starten“. Während der Linie: Haltestellen mit Fortschritt, Fahrgäste an Bord, Fahrpreise. |
| Firma | Firmenkasse, Stufe, Ruf, Bewertung, Fahrten, Kilometer, Auto, Schicht starten oder beenden, Verbindung wechseln. |

Nach jeder Fahrt erscheint die Abschluss-Karte (Geld, Sterne, Kommentar). Bei neuen Anfragen und Nachrichten vibriert
das Handy kurz. Der Bildschirm bleibt an, solange die App offen ist. Die Sprache folgt dem Spiel.

## Technik

- `src/core/companion.c`: was die App bekommt, als JSON (Auto, Geld, Angebote, Auftrag, Chat, Weg). Reines C, getestet.
- `src/companion_net.c`: der Webserver (Winsock im Spiel, POSIX für Tests). Endpunkte: `/` (die App-Seite),
  `/api/hello`, `/api/state`, `/api/lines`, `/api/history` (0.13: the ride book), `/api/do?a=accept|line|cancel|reply|refresh|shift|setting|buy|set|target&n=`, `/api/tile?z=&x=&y=`.
  Alles außer `/` und `/api/hello` braucht `k=<Code>`.
- `app/index.html`: die Oberfläche, ohne Bibliotheken. Die Mod liefert sie für den Browser aus, die App trägt
  dieselbe Datei in sich.
- `android/`: die Android-App (Java, ein WebView plus Suche im WLAN, Vibration, Bildschirm an). `build_apk.py` baut
  sie ohne Android Studio: Werkzeuge aus Maven Central, `AndroidManifest.xml` und `resources.arsc` schreibt das
  Skript selbst, Signatur v2. **`android/release.p12` aufheben:** Updates der App müssen mit demselben Schlüssel
  signiert sein. Er gehört nicht auf GitHub.
- `tests/app_demo.c`: der Server ohne Spiel, mit einem simulierten Auto, das Aufträge abfährt. Zum Ausprobieren
  der App am PC.

## Stand

Am PC getestet: Server, Code, Suche, alle Endpunkte, eine ganze Fahrt mit simuliertem Auto in der App-Oberfläche
(so wie die APK sie lädt), Manifest und Signatur der APK. **Noch nicht getestet:** auf einem echten Android-Handy
und im Spiel.

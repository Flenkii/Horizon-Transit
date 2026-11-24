# Machbarkeit: Horizon Transit (früher „FH2 Transport Simulator“)

Stand: 16.11.2026 · Spielversion: `forza_durango_release_final.exe` (Zeitstempel 0x565501ED)

**Wie die Bewertungen zu lesen sind:**

- **sicher:** im Spielcode bzw. in den Spieldateien nachgewiesen, *und* im Spiel schon einmal benutzt worden (zum Beispiel von Gripline, ANNA oder der Plattform selbst).
- **wahrscheinlich:** im Spielcode nachgewiesen, aber von dieser Mod im Spiel noch nicht getestet.
- **experimentell:** die Daten sind da, aber es ist unklar, ob das Spiel mitspielt.
- **unrealistisch:** nicht gefunden, oder nur mit sehr viel Aufwand machbar.

Ehrlich vorweg: **Nichts von dieser Mod ist bisher im Spiel getestet.** Getestet ist nur die Job-Logik auf dem PC: 2509 automatische Prüfungen, alle bestanden. Außerdem stimmt jede Speicheradresse, die die Mod im Spiel benutzt, Byte für Byte mit deiner `forza_durango_release_final.exe` überein.

## 1. Bewertung pro Funktion

| Funktion | Uber | Bus | Beleg |
|---|---|---|---|
| Position und Tempo des eigenen Autos lesen | sicher | sicher | Die Auto-Update-Funktion `game+0x2A9400` wird schon von Gripline, Underglow, NitroSystem und ANNA gehookt. Das eigene Auto erkennt man an `car+0x4D08 = 0` (die Spielfunktion bei `game+0xF62280` liest genau dieses Feld). Die Physikdaten kommen über `car+0x4C08` → vtable-Slot `0x1B8`. |
| Ruckeln, starkes Bremsen, Kurvenkräfte | wahrscheinlich | wahrscheinlich | Wird aus der Geschwindigkeit berechnet, die das Spiel liefert. Die Schwellenwerte sind geschätzt und müssen im Spiel noch eingestellt werden. |
| Unfälle erkennen | wahrscheinlich | wahrscheinlich | Erkannt an einem plötzlichen Geschwindigkeitssprung. Das Zurückspulen (Rewind) wird dabei herausgefiltert, das ist in den Tests geprüft. |
| GPS-Ziel automatisch setzen (Route auf der Minimap) | sicher | sicher | Die Mod nutzt die Spielfunktion `SatNavComponent::NavigateToPosition` (`game+0x6B58B0`). Gripline setzt das GPS beim „Summon“ genauso, und das hast du im Spiel getestet. |
| „Angekommen“ über das Spiel-GPS erkennen | wahrscheinlich | wahrscheinlich | Dieselbe Stelle, die Gripline benutzt. Zusätzlich gilt: weniger als 30 m Abstand zum Ziel zählt immer als angekommen. |
| Echte Haltestellen und Orte | sicher | sicher | Seit 0.8: 139 Orte aus den Spieldateien: das Festival und 7 Außenposten (`triggers.tz`), 11 echte Bushaltestellen (Schilder `fra_obj_sign_busstop` in `extras/Anthem.track`), 49 Parkplätze (`trackroutes/ParkingAreas.nt`) und 71 Rennstarts auf dem Land. Nichts davon ist erfunden. Im Internet gibt es keine Liste der FH2-Haltestellen (gesucht am 21.11.2026). Die italienischen Wartehäuschen stecken nur in den gepackten Geometriedateien (`GeoChunk*.minizip`) und sind nicht ausgelesen. Autobahnen erkennt die Mod nur an Schildern und Brücken, nicht vollständig. |
| Namen der Außenposten | wahrscheinlich | wahrscheinlich | Die Namen sind aus den Datenbank-Einträgen RoadTripLocations und DestinationHubs abgeleitet. Ob „antrani“ wirklich Castelletto ist, wurde im Spiel nicht bestätigt. |
| Virtuelle Fahrgäste, Zeitlimit, Bezahlung, Zufriedenheit | sicher | sicher | Das ist reine Logik der Mod und in den Tests geprüft (siehe `tests/test_core.c`). |
| HUD oben rechts (Zeit, Zufriedenheit, Sterne) | wahrscheinlich | wahrscheinlich | Läuft über die HUD-Ebene der Mod-Plattform (eine „Vorschau“-Schnittstelle). Bisher benutzt keine andere Mod diese Ebene. Wenn sie fehlt, kommt der Status alle 25 s als Pop-up. |
| Handy-Menü mit Aufträgen | wahrscheinlich | wahrscheinlich | Erscheint als Panel der HUD-Ebene. Dasselbe Menü gibt es zusätzlich im Pausemenü unter MODS. |
| Pop-ups und Töne im Stil des Spiels | sicher | sicher | Kommen von der Plattform (API 10, Toasts). |
| Die normale Minimap bleibt sichtbar | wahrscheinlich | wahrscheinlich | Nur das Handy-Panel liegt links und verdeckt die Minimap, solange es offen ist. Schließt du das Handy, ist die Minimap wieder frei. |
| Abseits der Straße fahren erkennen | unrealistisch (vorerst) | unrealistisch (vorerst) | Es gibt im Spiel eine Funktion „nächste Straße“, die Gripline benutzt. Die ist aber aufwendig und riskant, deshalb ist sie noch nicht eingebaut. |
| Geld auf das echte Spielkonto (Credits) | wahrscheinlich | wahrscheinlich | Seit 0.13 (23.11.2026): Das Spiel hat eine eigene Gutschrift-Funktion `game+0x9E9120` (rcx unbenutzt, edx = Betrag, gibt 1 bei Erfolg). Sie holt das aktive Profil (`game+0x34B3E18`), erhöht `Main/Credits` (über `game+0x2CC550` → `game+0x2F5EF0`, gedeckelt bei 999.999.999) und `Main/NumCreditsEarned` und speichert das Profil (`game+0x2F3820`). Die Geschenk- und Belohnungs-Handler des Spiels rufen sie auf. Nur aus der exe gelesen, im Spiel ungetestet. Ausgeben: `game+0x564800` (rcx unbenutzt, edx = Betrag; Profil über `game+0x34ABA60` → `game+0x21E070` → `game+0x2DDE80`, Kontostand `game+0x2DC820`, kein Speichern, prüft den Kontostand nicht). Seit 0.13 läuft alles Geld der Mod darüber. |
| Echte Fußgänger, die ein- und aussteigen | — | unrealistisch | FH2 hat keine Fußgänger. Die Fahrgäste gibt es nur als Zahlen und Texte. |

## 2. Der Bus

- **Das Bus-Modell ist vorhanden:** In der Spieldatenbank ist es `Data_Car` Id 414 `PG_BUS_14`. Es hat volle Physik-, Upgrade- und Farbeinträge, ist aber ein Verkehrsauto. Die Datei `PG_Bus_14.zip` ist ebenfalls da, mit Innenraum und Lenkrad.
- **Im Autohaus ist der Bus nicht.** Es fehlt sein Eintrag in `Data_Car_Buckets`, und er hat eine Inhaltslizenz (`ContentId 99`).
- **Das Risiko:** Seine `Locators.xml` ist leer (28 Bytes), und es gibt kein Fahrermodell. Kameras, Fahrersitz oder Cockpit könnten deshalb fehlen oder falsch sitzen, oder das Spiel stürzt ab.
- **Die Mod enthält dafür einen experimentellen Schalter**, „Bus im Autohaus“, der standardmäßig **aus** ist. Er ändert nur die Spieldatenbank im Speicher (über `add_sql` der Plattform) und legt vorher ein Backup des Spielstands an. Der SQL-Befehl wurde an einer Kopie der Datenbank geprüft. Ob der Bus danach im Spiel fahrbar ist, ist **ungetestet**.
- **Das Jobsystem braucht den Bus nicht.** Alle Buslinien funktionieren mit jedem Auto. Ein Transporter wie der Ford Transit SSV passt gut.

## 3. Empfehlung

**Zuerst den Fahrdienst (Uber) im Spiel testen, dann die Buslinien, und den echten Bus erst ganz am Schluss.**

- Uber und Bus benutzen dieselben Bausteine (Auto lesen, GPS, HUD).
- Wenn eine Uber-Fahrt im Spiel klappt, ist der größte Teil des Busmodus automatisch mitgetestet.
- Der echte Bus ist die einzige riskante Stelle und hängt nicht an den anderen Teilen.

## 4. Verträglichkeit mit Gripline und anderen Mods

- **Eigene DLL, eigener Ordner.** Die Mod liegt als `TransportSim.dll` im Ordner `fh2-mods` und legt ihre Einstellungen im eigenen Ordner `Azimuth\TransportSim\` ab. Keine Datei von Gripline oder einer anderen Mod wird verändert.
- **Kein Code von Gripline übernommen.** Der GPS-Aufruf sieht genauso aus wie bei Gripline, weil die Spielfunktion ihn genau so verlangt. Dafür gibt es nur diese eine Form.
- **Hooks:** Alle Hooks laufen über die Plattform. Die Plattform reiht sie hintereinander, sodass die Hooks von Gripline, ANNA usw. weiter funktionieren. Vor jedem Hook prüft die Mod die Bytes des Spiels. Passen sie nicht, bleibt der Teil aus und das Log sagt es.
- **Tasten:** F9, F10 und F11 sind frei. Extra Options belegt F1–F4 und Tab, Gripline F5–F8 und Buchstaben. Am Controller ist die Kombination LB+RB+(Steuerkreuz hoch / Steuerkreuz rechts / A). **Achtung:** LB ist bei NitroSystem der Nitro-Knopf, die Kombination kann also kurz Nitro auslösen. Alle Tasten lassen sich im Mod-Menü neu belegen.
- **HUD:** Es kann immer nur eine Mod die HUD-Ebene zeigen. Im Moment nutzt keine andere Mod sie.
- **GPS:**
  - ANNA (Autopilot) hängt sich an dieselbe GPS-Funktion. Beides zusammen gleichzeitig ist ungetestet.
  - Gripline-Summon löscht am Ende das GPS-Ziel. Mit F9 (Handy öffnen) wird das Ziel neu gesetzt.
  - Vor einem Rennen oder Event am besten den Auftrag abbrechen, weil sich sonst die GPS-Ziele in die Quere kommen können.

## 5. Türen beim Ein- und Aussteigen (Stand 17.11.2026, Version 0.4.3)

Wunsch: Hält man beim Fahrgast, geht eine Tür sichtbar auf (hinten links, hinten rechts oder vorne beim Beifahrer, zufällig), mit Türgeräusch, und wieder zu. Erst dann startet die Fahrt. Am Ziel dasselbe mit derselben Tür.

### Was die Plattform kann

- `fh2mod.h` und `fh2mod_hud.h` haben **keine** Funktion für Türen, Animationen oder eigene Geräusche. `play_ui_sound` spielt nur die 11 Menü-Geräusche des Spiels (Auswahl, Zurück, Fehler, Countdown, Foto ...). Ein Türgeräusch ist nicht dabei.

### Wie andere Mods das machen (nur die Technik, kein Code übernommen)

- **Extra Options für FH2** (RokkuDayo/ForzaHorizon2_ExtraOptions, `fh2/vehicle_controls.c`, `docs/VEHICLE_CONTROLS_NOTES.md`) öffnet und schließt im Spiel das **Dach**, mit derselben Technik, die das Spiel im Showroom (ForzaVista) für Türen, Haube und Kofferraum benutzt:
  - Jedes Auto hat einen Animations-Graphen: Animator bei `car+0x4D40`, Graph bei `animator+0x1C8`.
  - Der Graph kennt benannte Ereignisse wie `roof:open`, `roof:close`, `roof:toggle`. Für Türen heißen sie im Showroom (in FH3 belegt, gleiche Engine) `doorLF`, `doorRF`, `doorLR`, `doorRR` mit `:open` / `:close`.
  - Ausgelöst wird ein Ereignis mit der Spielfunktion `game+0xF5D300 (car, "name")`.
  - Wichtig: Im Freien Fahren läuft der Graph im Fahr-Zweig (Detail 1). Der reagiert nicht auf die Showroom-Ereignisse. Mit `game+0x16FBE90 (graph, 0)` schaltet man auf den Showroom-Zweig (Detail 0) um. Der Übergang dauert etwa 0,25 s.
  - Extra Options hat **keine Lizenzdatei** im Repository. Deshalb ist **kein Code** übernommen. Horizon Transit benutzt nur diese Fakten (Adressen und Ablauf) und ist komplett neu geschrieben. Die Ereignisliste liest die Mod selbst aus, ohne die Suchfunktion des Spiels.
- **Gripline Lite** (Flenkii/Gripline-Lite, GPL-3.0, für Forza Horizon 1 ReImagined, „wie Gripline für FH2“) spielt beim Aussteigen zwei WAV-Dateien mit Windows `PlaySound` ab, **während das Bild schwarz ist**. Die Tür wird dort also nur gehört, nicht als Animation gezeigt. Die Geräusche stammen laut Gripline Lite von „Sounds From The Lab“ (lizenzfrei).
- **Gripline für FH2 (V7.2.x) macht es genauso**, das zeigen `Gripline.log` und die Texte in `Gripline.dll` auf deinem PC (nur gelesen, kein Code übernommen): „DOORS: the game's door animation is there - getting out opens the door“, „animation detail of the car was 1 - full detail (0) for the door“, und die Ereignisnamen `doorLF:open`, `doorLF:close`, `doorRF:open`, `doorRF:close`. Damit ist bestätigt: Die Türnamen stimmen für FH2, und die Tür geht im Freien Fahren sichtbar auf, wenn der Graph auf Detail 0 steht.
- Laut Gripline-Log hat eine andere Mod (vermutlich FH2MP) die Ereignis-Funktion `game+0xF5D300` mit einem Hook belegt. Gripline ruft sie trotzdem auf, Horizon Transit genauso (über den Hook, der dann die Originalfunktion aufruft).
- Die Spieldatenbank (`gamedbRC.slt`, Tabelle `Data_Car` und alle anderen) hat **keine** Spalte für die Lenkradseite. Gripline erkennt die Seite über die Kamera: In der Fahrer- oder Cockpitkamera (Kamera-Objekte mit vtable `game+0x2E78120` bzw. `game+0x2E77C30`, Weltposition als drei doubles bei `+0x1A0`, Ansichtsfunktion `game+0x103C590` in vtable-Platz `+0x50`) liegt die Kamera beim Rechtslenker mehr als 10 cm rechts der Automitte. Horizon Transit macht das jetzt genauso (eigener Code, nur die Fakten aus Gripline.dll gelesen).

### Was geht, was nicht, was offen ist

| Teil | Stand |
|---|---|
| Tür auswählen (hinten links, hinten rechts, Beifahrer), gleiche Tür am Ziel | **gebaut und getestet** (ohne Spiel) |
| Ein- und Aussteigen dauert so lange, bis die Tür wieder zu ist | **gebaut und getestet** (ohne Spiel): 3,5 s statt 3 s bzw. 2 s |
| Losfahren mit offener Tür, Auftrag abbrechen: Tür geht sofort zu | **gebaut und getestet** (ohne Spiel) |
| Türgeräusch auf und zu | **gebaut**, die Geräusche stecken in der DLL (eigene WAVs in `Azimuth\TransportSim\sounds\` ersetzen sie). **Funktioniert im Spiel** (17.11.). |
| Tür wirklich öffnen (sichtbar, auch von außen) | **funktioniert im Spiel** (Marvin, 17.11., Jeep Wagoneer). Ob die Türen in FH2 dieselben Namen haben und im Fahrbetrieb aufgehen, zeigt erst der Test. Das Log schreibt für jedes Auto, welche Teile es öffnen kann. |
| Rechtslenker (Fahrer rechts, Beifahrertür links) | **gebaut, im Spiel ungetestet.** Fährst du mit einem Auto kurz (etwa 1 Sekunde) in der Fahrer- oder Cockpitkamera, merkt sich die Mod die Lenkradseite dieses Autos (`[Doors] Learned` in `TransportSim.ini`, Log: „the wheel is on the …“). Bis dahin gilt: Fahrer links. Von Hand geht es mit `[Doors] RightHandDrive=` (Namen wie `NIS_SkylineGTR_R34`), das hat Vorrang. Die Spieldatenbank hat keine Angabe dazu (geprüft). |
| Zweitürer | Nur die Beifahrertür (die Mod erkennt an den Animationen, ob es hintere Türen gibt). |
| Bus | **noch nicht.** Die Türen gibt es nur bei Uber-Fahrten. Der Bus des Spiels ist ein Verkehrsauto und hat wahrscheinlich keine Showroom-Animation. |
| Geräusch aus der Richtung der Tür, leiser von außen | **nicht möglich** mit `PlaySound`. Das Geräusch kommt immer gleich laut, egal wo die Kamera ist. Lautstärke: `[Doors] Volume=` (0 bis 100). |

### Risiken

- Die Mod ruft zwei Spielfunktionen auf, die sie nicht selbst über Bytes prüfen kann (noch keine Kopie der exe hier). Sie benutzt sie nur, wenn ihre anderen Byte-Prüfungen das richtige Spiel bestätigen, und schreibt die ersten 16 Bytes beider Funktionen ins Log. Damit kann die Prüfung im nächsten Schritt genau festgelegt werden.
- Während die Tür auf- und zugeht (rund 3 Sekunden, nur im Stand), läuft der Showroom-Zweig der Animation. Bei manchen Autos sieht dabei eventuell etwas anders aus (bei Extra Options z. B. Klappscheinwerfer). Danach schaltet die Mod zurück.
- Seit Marvins Test am 17.11. (Jeep Wagoneer: Tür und Geräusch funktionieren) ist der Schalter „Fahrgast-Türen“ standardmäßig **an**. Mit `[Doors] Animate=0` gibt es nur die Geräusche.

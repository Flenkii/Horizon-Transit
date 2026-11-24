# Changelog

What changed for players, newest first. Every version is on the [Releases](../../releases) page.

## 0.13.0

Real game money only. The company funds are gone, everything runs on your credits. Plus a lot the phone app can do without the game, bus engine swaps and the missing roads on the map.

- **Pay goes straight to your game account:** every ride, bus line, bonus and lost item pays real credits. The mod calls the game's own award function (`game+0x9E9120`), the same one gifts and rewards use. It raises your credits and "total earned" and saves the game.
- **Everything in CR:** prices, pay, tips and bonuses are shown in CR, five times the old euro amounts (`[Credits] Scale=5` in the INI). A ride over 5 km brings about 6,000 to 8,000 CR.
- **Shop with real credits:** you buy with your game account, through the game's own "spend credits" code (`game+0x564800`). The mod checks your balance first; if it's not enough, nothing is taken.
- **9 new shop items:** water & snacks, dashcam, punctuality bonus, long-distance licence, hotspot radar, night surcharge (10 pm to 6 am, PC clock), bus air conditioning, billboards, insurance. 18 items now, each with its own effect.
- **Safety:** the bytes of both game functions are checked before use. If they don't match, there are no credits and no shop, and the log says why. The platform backs up your save at every start.
- **Game account on the phone:** top right, in Account, in the Shop and in the phone app.
- **Fixed fare:** from pickup on, the ride panel shows the agreed price like Uber ("FARE"). Before, it counted up with the distance, which looked like fast driving paid more.
- **Driving like a driver app:** offline you only see GO. After GO it says "Looking for rides …" and every few seconds one request comes in. Each waits 20 seconds for a yes, with a countdown. Accept or decline, otherwise the next one comes. Works standing still and with the phone open, more often at hotspots. The list of three rides to choose from is gone.
- **Places with names:** instead of "Meeting point 4" or "Car park 7" all 131 places now have real names that fit where they are, e.g. "Place Garibaldi" or "Promenade des Anglais" in Nice, "Piazza della Cisterna" in San Giovanni, "Marina" in Castelletto. Made up; the positions are still the game's.
- **New design "Pure Dark":** like the Uber app in dark mode. Nearly black, grey cards, white text, light buttons. 9 designs now, on the game phone and in the app.
- **Pick lists:** settings with many values (Design, Device) open a list to pick from, on the game phone (Enter) and in the app, instead of clicking through.
- **Map: missing roads.** The map now shows every road from the game files. Before, e.g. the festival's west access and the coast road near Castelletto were missing (they were only drawn as barely visible dirt tracks, and the GPS didn't know some of them). The GPS uses them now too. The app reloads its saved map pieces once for this.
- **Bus engine swaps (experimental):** with "Bus in the Autoshow" on, the bus gets four engines under Upgrades > Conversion: 6.2L V8 (Corvette), 7.0L Hemi V8, 6.0L V12 (Enzo) and 5.2L V10 (Gallardo). Like other car swaps, but only through the mod's database questions; the database file stays untouched. Fool-proof: the four engines are copies of real conversion entries of other cars with the same number, so your save remembers an engine the game itself knows. Switch off, mod removed or online: the bus keeps a valid engine.
- **Phone app without the game:**
  - **Earnings** in Account like a driver app: the week as a bar per day, tap a day to see it, arrows for older weeks.
  - **Ride book** with every ride and bus line: time, route, distance, duration, tip and car. The mod keeps it in `TransportSim-rides.txt` (the last 300), the app keeps it without the game.
  - **Progress:** all tiers and what they need, the shop items still missing, the bus lines you've driven.
  - **Timetables:** in the Bus tab, "Timetable" shows every stop with minutes from the start, "On the map" shows the line on the road.
  - **GPS target:** tap the map, "Set as target". If the game runs, the GPS sets it at once (after a job again), otherwise at the next start. It switches off when you get there.
  - **Versions:** the Options show small at the top which mod version and which app version you have.
- **Game phone polish:** the tab highlight is a pill and no longer sticks out of the bar, long tab names get smaller instead of cut off, all amounts with thousands separators ("5,130 CR"), centred one-line footer, "Tier: Bronze · 10 to go" and the GO card fit. Arrow keys: at the bottom of a tab it wraps to the top instead of jumping to the next tab.
- **App polish:** two lines for the target, ring lines say "Ring line via …", "10 rides to go", buy buttons in a row line up on the tablet, "5 %" and "CR" no longer wrap, a line of the hidden notice no longer flashes at the top edge. Fixed: with more than 8 designs the app would have cut off the design choice.
- Phone app 0.13.0 (build 23). Install it over the old one and tap "Update"; the saved connection stays.

## 0.12.0

The game phone now looks like the phone app. Checked as pictures in all designs on phone, Edge and tablet.

- **New header:** logo, name and your car on the left, your money as a glass pill on the right.
- **Tab bar at the bottom:** floating, with icons (car, bus, map, person, bag, gear); the highlight glides along.
- **Big page titles** with a small badge on the right (hotspot, "Bus ready", money).
- **Ride requests** as big cards with the pay large on the right; the selected one gets a coloured edge and an "Accept ride" button with the pay.
- **Account:** the shift as a GO card with a round GO button ("You're offline" / "You're online").
- **Options:** on/off as Android-style switches.
- **Job:** a sheet at the top with target, big speed and progress; short values as tiles side by side; "Cancel job" as a red button.
- If the content doesn't fit, the page tightens up instead of cutting off.

## 0.11.1

The map in the phone app often stayed empty (dark blue) when zooming in.

- **Map loads reliably:** the app fetched too many map pieces at once. With the game running the PC couldn't keep up, pieces timed out and the app briefly thought the PC was gone. Now at most 6 pieces are on the way at once, a slow piece no longer means "PC gone", and it's fetched again after 1.5 s.
- **Always something to see:** under the sharp pieces the app first loads a coarser level, so zooming is never blank.
- **Faster:** pieces already on the phone come from the phone instead of the PC. Phones with a very high resolution fetch only a quarter as many pieces.
- **Hotspot zones:** zoomed in close they only tint the map slightly, so the roads stay readable.

## 0.11.0

A new look for the phone app and the game phone, inspired by Liquid Glass and Material You.

- **Glass instead of boxes:** cards, tab bar, map buttons and notices are translucent glass with a light edge and shine, over a soft background in your design's colours that moves slowly.
- **Floating tab bar:** a pill at the bottom, in landscape a bar on the left edge, with a gliding highlight.
- **Big numbers, round shapes:** bold headings, big amounts, round buttons and Android-style switches for every on/off option.
- **Like a ride-hailing app:** during a job the ride card slides over the map like a sheet, and a sign with minutes and distance floats on the map. "Accept ride" is a big pill with the pay.
- **Swipe instead of tap:** "Cancel job" is a red slider, so a stray tap while driving can't cancel the job.
- **GO button:** a big round GO button above the rides starts the shift; during the shift it shows what you earned today.
- **Hotspots as zones:** on the map, hotspots glow as coloured zones with their surcharge (e.g. 1.5×), the redder, the more.
- **Game phone in the same style:** glass cards, shiny selection, capsule tab bar and coloured lights in the background, also on tablet and Edge.

## 0.10.4

The phone app works without the game.

- **Without the game:** the app shows the last state (money, account, shop, bus lines, options, design) with "Game not connected · as of …" at the top. Rides, starting a line, the shift and buying need the game.
- **Change options without the game:** you can change every option in the app, the design switches at once. When the game runs again with the phone app on, the app reconnects by itself and the mod takes over the changes ("2 changes applied in the game").
- **Map without the game:** the app keeps every map piece it got on the phone. Options > "Offline map" > "Save map" fetches the whole map at once (about 600 pieces). The pictures come from your own game files and stay on your phone only.
- **Straight into the app:** with a saved PC, the app opens the tabs instead of the connect screen.
- API: `/api/do?a=set&n=<row*16+value>`, `idx` and `choices` per option in `/api/state`, all designs (`themes`) in `/api/lines`.

## 0.10.3

The phone app uses the space in landscape and on tablets.

- **Landscape:** the tab bar moves to the left edge and the header gets slimmer, leaving more height for the content.
- **Job in landscape and on tablets:** the big map (navi or map) on the left; target, speed, mood, time, chat and cancel on the right.
- **Lists in columns:** rides, shop, account, bus and options run in two or three columns from 700 pixels width.

## 0.10.2

Navigation in the phone app.

- **Map right in the job:** after accepting a ride or bus line, the Job tab shows the map with route, target and car at the top, speed, time, mood and chat below.
- **Navi or map:** top right on the map choose "Map" (north up) or "Navi" (turns with the car, a compass shows north). The app remembers the choice; Navi is the default.
- **Chat no longer flickers:** the app only changes what changed; a new line fades in once. Buttons in shop, options and account are no longer rebuilt all the time (a tap sometimes got lost).
- **Smooth:** car, rotation and the switch between map and navi glide instead of jumping. During a job the app updates five times a second.

## 0.10.1

Switch the phone app on from the game phone, design sync, all options, bigger shop and account in the app.

- **"Connect phone app" on the game phone:** F9, right to Options, the new line at the top. Enter switches the app on; then the line shows the code and the PC's address below. No need for the mod menu any more (its switch stays).
- **Design in sync:** change the design on the game phone and the app follows at once, including the colour of Android's status and navigation bars.
- **All options in the app:** an Options tab with the same rows as the game phone. Tapping changes them in the game. The game phone got 7 new rows and scrolls.
- **Bigger shop:** tip box (more tips), customer file (regulars more often), shift planner (double day's-goal bonus), ticket machine (more pay per km on bus lines). In the app: cards with icons, progress to the purchase, tap twice to buy.
- **Account in the app:** money, tier with progress, reputation, rating, shift, statistics to fold open (distance, passengers, tips, 5 stars, best ride, on time, crashes, regulars …) and the logbook.
- **Update:** install the new APK over the old one and tap "Update". Same app, same key, the saved connection stays.

## 0.10.0

Horizon Transit as a real phone app (prototype). All about it: [docs/PHONE-APP.md](docs/PHONE-APP.md) (German).

- **Android app** `HorizonTransit.apk`: accept rides, start bus lines, answer passengers, cancel a job, shift on and off. Live: job, speed, speed limit, mood, time left, meter, chat, end card, money, reputation, tier.
- **Map in the app:** drawn on the PC from your game files, with swipe and pinch zoom, car arrow, the way to the target along the roads, bus stops and hotspots.
- **New switch "Phone app (Wi-Fi)"** in the mod menu, off at first. Switched on, the mod starts a small server in your home network (port 8710, discovery on 8711) and shows the address and pairing code in the mod menu and as a pop-up. Nothing works without the 4-digit code (`[PhoneApp] Code` in the INI).
- The app finds the PC by itself. Without the app it also works in the phone's browser (iPhone too): `http://<address>:8710`.
- New tests for the app's data and actions. `tests/app_demo.c` runs the server without the game, with a simulated car.

## 0.9.2

Language like Gripline, English file names.

- **Language follows only the game:** Forza in German, the mod is German. English or any other game language, it's English. Before, in the first seconds after start (before the game picked its language) Windows' language counted; now it's English until then. The setting "Language" (Game, Deutsch, English) stays, default "Game".
- **English file names:** docs/TESTANLEITUNG.md is now docs/TESTING.md, MACHBARKEIT.md is FEASIBILITY.md, IDEEN-1.0.md is IDEAS-1.0.md, ROADMAP_de.md is ROADMAP.md, docs/bilder is docs/images.
- **Checked:** the whole test suite also ran with English forced, and every text of phone, speedo panel, maps, passenger messages and pop-ups was searched for German words: none found.

## 0.9.1

A navigation view on the phone's map, and smooth animations everywhere.

- **Navi zoom:** a new zoom level "Navi" (map tab: Near, Region, Whole map, Navi; during a job: Route, Whole map, Navi). The map turns with the car like the game's navigation in the bottom left: the way you drive always points up, you sit in the lower part, the road ahead is in view. The blue dot becomes a navigation arrow, and a compass in the top right shows north.
- **Smooth zoom changes:** switching zoom glides to the new view in 0.6 seconds (area, size and rotation). One spot of the map stays in place, so nothing swings away. The dot morphs into the arrow, and the compass fades in and out. A jump of the job map's area (the next target) glides too.
- **Calm navi rotation:** the map follows your heading smoothly, without jitter from small steering moves.
- **Nothing pops up any more:** when the phone shows something else (a tab, the selection, rows, pins), the old picture crossfades into the new one in 0.22 seconds, while the map keeps gliding underneath. The tab highlight slides to the new tab (the tab bar itself doesn't fade, so there is only one highlight).
- **Closing the phone:** it fades out to the left (before, it vanished at once). Opening still slides it in.
- **Ride panel and passenger lines:** a new target, wish or speed limit crossfades. New passenger lines fade in, the last one fades out.
- **Gradients:** the beam at your dot fades out softly, the map has a gentle shade at the top and bottom, and the compass has a gradient.
- New tests: gliding, turning, crossfades, the tab highlight. `tools/anim_gifs.py` makes GIFs of every animation from the test frames.

## 0.9.0

The phone's map now comes from the game itself. Every player has it, with nothing but the DLL.

- **Map from your game files:** at start the mod reads the pause menu's map picture (`media\ui\textures\data_bound\Anthem_Map.zip`) from your own game folder and restyles it as a dark phone map (land, forest, fields, towns, water). Nothing of the game is in the mod. `Karte.bmp` is no longer needed (it is only a fallback if the game files can't be read).
- **The game's road network:** roads come from the game's own network (`media\tracks\anthem\ribbon_00\anthem_00.nav`): streets, dirt roads, trails and motorways (amber). Zoomed in they are drawn sharp, and the ways on the phone follow them.
- **Placed to the metre:** the map sits where the game's pause menu puts it (`MapProfilePauseMenu.xml`); the game's roads lie exactly on the roads in the picture, so your dot is where you are.
- The map is built on its own thread in the first 1 to 2 seconds; until then the phone shows the places on a dark background. The log says what was read (`map: from the game's files: ...`).
- New tests: unzipping (deflate), BC7, the Xbox One tile order, the road network, motorways, ways along the roads.

## 0.8.1

Fixes from the first test in the game.

- **Answering works with a controller:** LB + RB + D-pad down (change it under Pause › MODS, or `[Keys] ReplyPad`); B on the keyboard as before. Passengers now always ask their questions. Before, they only asked after you had driven with the keyboard, so controller players never got one. The prompt shows the right button, and you have 12 seconds instead of 9.
- **Push requests without "Start shift":** the shift now starts by itself with your first ride or line, like going online in a ride app. After that, requests pop up while you drive around without a job.
- **Fresh requests on the phone:** during a shift the phone often kept the old list of requests, even after a cancelled ride. Now only a push request's list stays for a minute.
- **Regulars and lost items more often:** regulars ask for you in 25 % of lists (was 18 %), lost items after 15 % of rides (was 7 %).

## 0.8.0

More realistic stops and destinations.

- **Real bus stops:** the 14 bus stop signs the map makers put into the game (`extras/Anthem.track`) are now stops: 8 in Nice (signs on both sides of a street count as one), 2 in Saint Martin, 1 in Montellino. The **Nice city bus** now runs through five of them.
- **Real car parks:** 49 of the game's parking spots (`trackroutes/ParkingAreas.nt`) in Nice, Sisteron, San Giovanni, Castelletto and along the coast are pickups and destinations.
- **Race starts only in the countryside:** in towns with enough real places they are gone, and so is every one near a motorway sign or motorway bridge. Rides pick real places 90 % of the time, shared rides too.
- **Car meet:** nothing within 500 m of the car meet (on the coast south of the festival) any more.
- **Festival stop on the road:** it is no longer the garage zone in the middle of the grounds but the road through the festival.
- A few more people get on and off at real bus stops than at a spot in the countryside.
- 139 places instead of 208, all from the game's files. `tools/gen_stops.py` now takes the game's `media/tracks/anthem` folder.

## 0.7.1

- Map: re-measured the alignment (against every race start). Your own position and the Horizon Festival now sit where the game shows them (they were off by a few hundred metres).
- Map: roads under the icons and place names, and the festival grounds, are filled in from a second map picture instead of being smudged out.
- Map: the car's arrow follows the direction of travel while driving; standing still, the mod learns which way the game's forward vector points.
- Karte.bmp has to be copied again (new alignment).

## 0.7.0

A map of the festival region on the phone, in a dark maps-app style.

- **New "Map" tab:** you as a blue dot with your heading, the towns, the festival and the hotspots (orange circles with their surcharge). Enter zooms: near (about 3 km), region (about 9 km), the whole map. A scale bar bottom right.
- **Preview before accepting:** Enter on a ride first shows it on the map: you, the pickup (green dot), the destination (red pin) and their names. Blue is the way to the pickup, grey the ride after it; shared rides also show the second rider and both destinations. Below: "Accept ride" (with the fare) and "Back" (↑↓ to choose, Enter). ←/→ switch between the route and the whole map, Backspace goes back to the list.
- **Bus lines on the map:** Enter on a line shows all its stops, numbered in driving order, and the line's way, with "Start line" and "Back".
- **Map during a job:** ←/→ switch the phone between "Job" and "Map". The map shows the way to the next target in blue and the rest in grey (bus: done stops grey, the next one blue). Enter zooms there; cancelling only works on the Job tab.
- **Ways along the roads:** the lines follow the map's roads (shortest way on the road lines of the picture), bridging small gaps; the game's GPS still guides the actual driving.
- **The map picture is not part of the mod.** The mod reads `Karte.bmp` (8-bit BMP) from its folder (`Azimuth\TransportSim\`). Make it from a picture of the game's map you are allowed to use with `tools/make_map.py` (and `tools/clean_xboxdome.py` for one particular picture). Without it the map shows the places and routes on a plain dark background.
- New mod-menu setting **"Map preview before accepting"** (on by default). Off: Enter accepts at once, as before.
- Tabs use a slightly smaller font so six fit. Backspace now closes the preview (or the job's map) before the phone.
- 9813 automated checks pass, with new ones for reading the map, the ways, the Map tab, the preview and the job's map.

## 0.6.0

Seven new things to make Horizon Transit feel like a real ride app. Tested automatically only, not in the game yet.

- **Card after the ride:** at the destination the ride card by the speedometer shows the result for 8 seconds: the stars light up one by one, the money counts up, a short comment from the passenger (42 comments by stars) and the fare, tip, streak bonus and whether you were on time. After a bus line too. The payout pop-up then goes away (the till sound stays). With the ride panel off, everything stays as before.
- **Shift review:** ending a shift shows a card for 14 seconds: earnings, rides and lines, average stars, distance, tips, the best job, the reputation change, the day's goal and your driver tier as a medal.
- **Animations:** the ride card slides up and fades in and out, the route fills smoothly, on the cards the money counts up and the stars pop one after another. The phone slides in from the left when it opens.
- **Ride classes by car:** small cars and hot hatches get *Saver* rides (x0.85, easy-going passengers, a bit more time), off-roaders *XL* rides with 2 to 4 people (x1.3), luxury saloons and supercars from the Silver tier *Black* rides (x1.4, very strict about rough driving). Ordinary *Comfort* rides keep coming too. The class shows on the phone and as a badge on the ride card.
- **Hotspots:** one or two places (the festival or towns with enough spots) are busy for 4 to 7 minutes: requests within 1.5 km pay 1.2x to 1.5x. Near one, the first request usually comes from there. The phone shows the nearest hotspot at the top of the ride list, the ride card the surcharge by the meter. On shift a pop-up names new hotspots nearby. Class and surcharge together at most x2.
- **Driver tiers:** Bronze, Silver (10 jobs, 4.0 average, reputation 55), Gold (30, 4.3, 68), Platinum (75, 4.6, 82). Silver unlocks Black rides, Gold gives +5 % pay and more VIP guests, Platinum +10 % pay and +0.1x in hotspots. A tier is never lost (saved as `[Career] Tier`). A new tier gets its own card with a medal. The account shows the tier with a bar to the next (instead of the tips row; tips moved to the account's footer).
- **Shared rides:** now and then the medium request is a shared ride with two passengers: you pick up the first, the second joins on the way (they say hello), the first gets out at their destination and pays, then you drive the second on. Each pays 20 % less than alone, together it pays off. Wishes only start once the first is out. At the end the card shows both names and the total.
- Saver/XL/Black, hotspots, tier perks and shared rides are off when "Reputation, special rides, wishes" is off.
- 9504 automatic checks pass (with pictures), including new ones for cards, classes, hotspots, tiers and shared rides.

## 0.5.1

More sound, more realistic wishes, and you can talk back.

- **Speed warning tones:** when the speed sign turns yellow, one soft "bong" plays once (again only after you were back under the limit for 1.5 s). While the sign is red, a "ding-ding" chime repeats every second; back to yellow, it stops. Switch: "Speed warning tones" in the mod menu.
- **New sounds**, all synthesised by the mod's own script (`tools/gen_sounds.py`, no recordings, nobody to credit) and built into the DLL: a gong before every bus announcement, the stop bell a few seconds after it, a ping for push requests, a cash chime on every payout.
- Sounds now play through waveOut, several at once. New mod-menu setting **"Mod sounds"**: Off / Quiet / Normal (default) / Loud; Off brings back the game's menu sounds. WAVs with the same name in `TransportSim\sounds\` replace the built-in ones.
- **Wishes last until the destination** instead of a few seconds: "under 70" sets the speed sign to 70 for the rest of the ride. A little over is fine; clearly over (10 km/h for 2 s) gets a reminder, the second time the wish fails. "Smooth" forgives one jolt with a remark. The chip reads "Till end: under 70" instead of a timer.
- **"Floor it" in a supercar:** in modern supercars and hypercars some passengers (car fans and day-trippers most of all) want you to step on it: no speed limit for the rest of the ride, the sign turns into the end-of-restrictions sign, and hitting 180 km/h (110 mph) fulfils the wish.
- **Talk back:** now and then a passenger asks you something and the ride panel shows "[B] Reply (9)". Press B and your driver answers (in the feed, and with Voices on out loud in his own fixed, low male voice that no passenger has); the passenger is pleased (+3 % satisfaction) and reacts. Staying quiet is fine too. At most two questions a ride. Keyboard only: questions only come while you drive with W/A/S/D or the arrow keys, so controller and wheel players never see a prompt. Other key: `[Keys] ReplyKey` in `TransportSim.ini`.
- **Nicer app:** thin meters under the entries: progress to the pickup and the destination, the line's progress, satisfaction and reputation in green, yellow or red.
- **New design "Pure"** in the style of ride-hailing apps: white, black type, grey cards without outlines, black selection. Phone > Options > Design or the mod menu. On the tablet the destination is now a square instead of a dot, as in ride apps (all designs).
- **New ride card by the speedometer:** the panel at the bottom right is now a card like a ride app's: the time left in big digits, where to (town big, spot small), a route from a dot to a square that fills as you drive (bus: the line's stops), the passenger's mood, the speed sign and a small G-meter on the right, the wish and the meter along the bottom.
- **Ride panel and passenger lines in the phone's design, as frosted glass:** both follow the chosen design (Pure too). The glass is milky and barely see-through, with a fine grain and a soft texture, a light rim and a soft sheen. Light designs get light glass with dark text. (An overlay window can't reliably blur the game behind it, so the blur is imitated.)
- 9063 automated checks. **Not tested in game yet.**

## 0.5.0
The first step towards 1.0: 15 ideas from the author's wishes, from players' wishes for taxi and bus games found online, and from real ride and bus apps. The whole list, where each idea came from and what couldn't be done: [docs/IDEEN-1.0_de.md](docs/IDEEN-1.0_de.md) (German).
- **Two new devices**: besides the phone there's the **Edge** (slim bezels, punch-hole camera, clock on the left, Android's three keys) and the **tablet** (landscape, the list on the left, details on the right). Switch in phone > Options > Device or in the mod menu. All 7 designs fit every device.
- **The tablet's column**: before you accept, the passenger's profile (age, job, rides together, favourite cars), pickup, destination, time limit and pay. On the Bus tab the line's stops as a route map; during a line, the stops done in grey and the next one marked. On Account the logbook with your last 5 jobs.
- **Bus lines both ways**: a line starts at the end that's nearer. Ring lines go the other way round every second time.
- **Stop announcements**: "Next stop: …", at the last one "Last stop, all change please." Read out with Voices on. The "Next stop" pop-up is gone instead.
- **Stop requests**: if nobody wants to get off or on, the announcement says "No stop requested" and you may drive through. If someone wants off, you hear the stop button.
- **Requests pop up on shift**: driving around without a job, a request comes as a pop-up every 2 to 3 minutes; the phone then shows it first. No shift, no pop-ups.
- **Regulars ask for you**: "Lena · asked for you", 15 % more pay.
- **Lost items**: now and then a passenger calls after getting out because something's still in your car. Bring it back: 60 € and +1 reputation.
- **Streak**: from the third 4- or 5-star job in a row, 5 % bonus per job, up to 20 %.
- **Day's goal**: 5 jobs with 4 stars on average in one shift pays 250 €.
- **Taxi meter by the speedometer**: the ride panel shows the fare running. On the bus it shows the fares taken.
- "Bus lines need the bus" left the phone's Options (room for "Device"); it's still in the mod menu.

## 0.4.3
- **Passenger doors (on by default)**: when a passenger gets in or out, a door opens and closes again with its sound: rear left, rear right or the front passenger door, at random. At the destination the same door opens again. The door really opens (the car's own showroom animation, also seen from outside). Cars without a door animation only play the sound. Getting in now takes 3.5 s (before 3 s), getting out 3.5 s (before 2 s). Switch: "Passenger doors" in the mod menu.
- **Right-hand drive**: drive a car for about a second in the driver or cockpit view and the mod remembers which side its wheel is on; in a right-hand-drive car the front passenger door is on the left. Until then the wheel counts as left. You can also list cars in `TransportSim.ini` under `[Doors] RightHandDrive=`.
- **Still one file**: the door sounds are built into `TransportSim.dll`. Own sounds: put `door_open.wav` / `door_close.wav` into `Azimuth\TransportSim\sounds`. Settings in `TransportSim.ini`, section `[Doors]`: `Animate` (0 = sound only), `ShutDelayMs`, `Volume`.
- **The speed-limit sign turns red now**: yellow and flashing when you're a little too fast, steady red from 10 km/h (6 mph) over the limit. Before, it only turned red past 15 km/h and flashed, so you hardly ever saw red.

## 0.4.2
- **Place and bus line names in English** on an English game: "Nice · Spot 4" instead of "Nizza · Treffpunkt 4", "Coast Line" instead of "Küstenlinie".
- **The old HUD list is gone** from the phone designs: the phone is always the drawn smartphone.
- **Many more passenger lines**: about 1480 different lines (before: about 790), with new topics by passenger type (in a hurry, fan, relaxed) and area (town, festival, countryside).
- **No repeats**: every line of a kind comes once before any comes back, and no line comes twice within 5 minutes.
- **Long lines aren't cut off** any more: they continue on a second feed line.
- **New switch "Passenger complaints"** (phone > Options and the mod menu): Off = no remarks about braking, speed, crashes or being late. The rating still counts.
- **Four new phone designs**: Ocean, Carbon, Riviera and Gold (besides Night, Light and Festival).
- **The "Phone with arrow keys" switch is gone**: the arrow keys, Enter and Backspace always work the open phone. Its place in Options now holds "Voices".
- **Passenger voices (experimental, off by default)**: passengers read their lines out loud with the Windows speech voices, each person with their own pitch. Mod menu: Off, Quiet, Normal, Loud.
- Passengers say their speed limit in mph too on an English game (before: km/h).
- **Ride panel by the speedometer**: while a passenger rides, bottom right: the speed-limit sign, a G-meter (keep the dot inside the circle: outside means braking, accelerating or cornering too hard), time left, the passenger's mood and wish. The sign at the top left is gone, and during a job the top of the screen stays clear. Before pickup the panel shows "Pick up" and the distance. Its place can be moved (RidePanelRight / RidePanelBottom in TransportSim.ini), and it can be switched off in the mod menu.
- **More varied rides**: places of your last rides are avoided, and the destination is usually in another area than the pickup. Now and then a quick hop (under 1.4 km) or a long trip across the map (7 to 14 km).
- **Much calmer passengers**: higher limits for gas (6.0 instead of 4.5 m/s²), braking (8.5 instead of 6.5) and cornering (8.5 instead of 6.5), 15 instead of 10 km/h leeway on speed, a harsh moment counts only after 0.6 s, remarks at most once a minute. The G-meter shows the new limits. The speed-limit sign flashes yellow when you're a little too fast and red once you're past the 15 km/h leeway.
- **Passenger lines always visible**: when the platform's HUD layer is off, the mod shows the lines itself, bottom left above the minimap.
- **Not everyone complains**: each passenger is quiet, now-and-then or outspoken. Quiet ones hardly say anything; sometimes you just see what they think ("Lena (thinks): Hm. That was a bit rough."). Thoughts are never read out. The rating still counts.

## 0.4.1
- **mph or km/h, like the game**: the mod reads the game's Units option (on Default, the game's language). An English game shows speeds in mph (rounded to 5) and distances in miles and feet.
- **Speed-limit sign**: while a passenger is aboard, a real round sign (white disc, red ring, the number) shows the limit top left instead of the "Max 120 km/h" text. It flashes yellow when you're too fast.
- **Mod menu in the game's language**: settings, choices, descriptions, key names and the mod's description under Pause > MODS > YOUR MODS are in English, and in German when the game is German (before, every line showed both languages). Translations live in `l10n/de.po`.

## 0.4.0
- **Your company**: reputation 0–100 (changes pay and which rides come), company funds instead of "credits", long-term statistics (tips, punctuality, wishes, special rides, records).
- **Shop** in the phone: dispatch radio (more time), advertising (always 3 requests), comfort pack (happier passengers), bigger bus (60 seats), premium clients (more VIPs).
- **Workday**: start a shift and end it with the day's statement.
- **Car groups** from the game's database: passengers like or dislike groups; special rides by group: VIP, business client, wedding couple, day trip.
- **Memory**: regulars remember your last car and how the last ride went.
- **Wishes on the way**: slower, smoother, faster, or a quick stop with the GPS.
- **Demand by area**: busy in towns and at the festival, quiet in the countryside.
- **Bus timetable** with delay per stop and a punctuality summary.
- **Options tab**: all settings in the phone too.
- **More forgiving passengers**: higher limits, 10 km/h leeway, short twitches don't count, remarks at most every 35 s.
- **Traffic bus in the Autoshow** (experimental) rebuilt: works with the FH2MP launcher, the database file stays untouched.
- One switch for reputation, special rides and wishes (off = like 0.3).

## 0.3.0
- The phone as a drawn smartphone on the left, in 3 designs.
- 6 bus lines instead of 3. 40 passengers with a story; regulars know you. About 640 lines.

## 0.2.0
- Speed limit chip, passenger kinds, car recognition, bus lines need the bus, phone tabs.

## 0.1.0
- First version: rides and 3 bus lines, GPS, satisfaction, pay, account.

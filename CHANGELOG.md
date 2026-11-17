# Changelog

What changed for players, newest first. Every version is on the [Releases](../../releases) page.

## 0.4.2
- **Place and bus line names in English** on an English game: "Nice · Spot 4" instead of "Nizza · Treffpunkt 4", "Coast Line" instead of "Küstenlinie".
- **The old HUD list is gone** from the phone designs: the phone is always the drawn smartphone.
- **Many more passenger lines**: about 1480 different lines (before: about 790), with new topics by passenger type (in a hurry, fan, relaxed) and area (town, festival, countryside).
- **No repeats**: every line of a kind comes once before any comes back, and no line comes twice within 5 minutes.
- **Long lines aren't cut off** any more: they continue on a second feed line.
- **New switch "Passenger complaints"** (phone > Options and the mod menu): Off = no remarks about braking, speed, crashes or being late. The rating still counts.
- **Four new phone designs**: Ocean, Carbon, Riviera and Gold (besides Night, Light and Festival).
- **Phone with the mouse** (mod menu, on by default): click a tab, click an entry to pick it and again to take it, the wheel scrolls. When the game hides the mouse pointer, the phone draws its own.
- **The "Phone with arrow keys" switch is gone**: the arrow keys, Enter and Backspace always work the open phone. Its place in Options now holds "Voices".
- **Passenger voices (experimental, off by default)**: passengers read their lines out loud with the Windows speech voices, each person with their own pitch. Mod menu: Off, Quiet, Normal, Loud.
- Passengers say their speed limit in mph too on an English game (before: km/h).
- **Ride panel by the speedometer**: while a passenger rides, bottom right: the speed-limit sign, a G-meter (keep the dot inside the circle: outside means braking, accelerating or cornering too hard), time left, the passenger's mood and wish. The sign at the top left is gone. Its place can be moved (RidePanelRight / RidePanelBottom in TransportSim.ini), and it can be switched off in the mod menu.
- **More varied rides**: places of your last rides are avoided, and the destination is usually in another area than the pickup. Now and then a quick hop (under 1.4 km) or a long trip across the map (7 to 14 km).
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

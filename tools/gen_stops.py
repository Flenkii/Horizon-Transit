#!/usr/bin/env python3
"""Generates src/core/stops_data.c from the game's own trigger zones.

Every position in the generated table comes from Forza Horizon 2's files, nothing is invented:
  media/tracks/anthem/triggerzones/tz_race_activations/triggers.tz
    - garage_spatial_zone / autoshow_spatial_zone ........ the main Horizon festival
    - <town>_destination_spatial_zone / _trigger_zone ..... the festival outposts
    - race_trigger_zone_rtNNNN ............................ race starts (they sit on roads)

Usage:
  python tools/gen_stops.py <path to triggers.tz> > src/core/stops_data.c

Outpost names: the trigger zones use internal names (antrani, montichello, ...). Which in-game outpost
each one is was matched by the game's own road-trip cutscene names in the game database
(RoadTripLocations: ArriveAmalfi, ArriveMonti, ArriveSanGimi, ArriveGorbio, ArriveWNice, ArriveSisteron)
and the destination pictures (DestinationHubs: Casteletto, Montellino, San_Giovanni, Saint_Martin,
Nice_Massena, Sisteron). "alpesdehaute" has no road-trip entry and keeps a neutral name.
That match is a reading of the data, not confirmed in game: docs/MACHBARKEIT.md says so too.
"""
import math
import re
import sys

# internal zone name -> (shown name, kind) ; kind 0 = main festival, 1 = outpost, 2 = place
SITES = [
    ("garage", "Horizon Festival", 0),
    ("antrani", "Castelletto", 1),
    ("montichello", "Montellino", 1),
    ("sangimignano", "San Giovanni", 1),
    ("eastnice", "Saint Martin", 1),
    ("westnice", "Nizza", 1),
    ("sisteron", "Sisteron", 1),
    ("alpesdehaute", "Alpes de Haute", 1),
]

# Bus lines: stop order by site key; legs get up to two stops in between from race starts on the way.
LINES = [
    ("1", "Festival-Ring", ["garage", "sangimignano", "montichello", "antrani", "garage"]),
    ("2", "Küstenlinie", ["garage", "sangimignano", "eastnice", "westnice"]),
    ("3", "Bergexpress", ["eastnice", "alpesdehaute", "sisteron", "westnice", "eastnice"]),
]

# Town buses: a round from the town's outpost through its meeting points (race starts named after it) and back.
TOWN_LINES = [
    ("4", "Nizza Stadtbus", "Nizza", 9),
    ("5", "Castelletto-Shuttle", "Castelletto", 8),
    ("6", "Weinberg-Linie", "Montellino", 8),
]


def town_line(places, town, count, spacing=350.0):
    """places: [(shown name, kind, pos)]. Starts at the town's outpost, then always the nearest meeting point of
    the town at least `spacing` metres from every stop so far, and back to the outpost."""
    home = next(i for i, p in enumerate(places) if p[0] == town and p[1] == 1)
    points = [i for i, p in enumerate(places) if p[1] == 2 and p[0].startswith(town + " · ")]
    stops = [home]
    while len(stops) < count - 1:
        last = places[stops[-1]][2]
        free = [i for i in points if i not in stops and all(flat(places[i][2], places[j][2]) >= spacing for j in stops)]
        if not free:
            break
        stops.append(min(free, key=lambda i: flat(places[i][2], last)))
    stops.append(home)
    return stops


def read_zones(path):
    text = open(path, encoding="utf-8-sig").read()
    zones = {}
    for attrs, x, y, z in re.findall(
        r'<triggerzone([^>]*)>\s*<position x="([-\d.]+)" y="([-\d.]+)" z="([-\d.]+)"', text
    ):
        name = re.search(r'name="([^"]*)"', attrs).group(1)
        zones[name] = (float(x), float(y), float(z))
    return zones


def site_position(zones, key):
    for suffix in ("_spatial_zone", "_destination_spatial_zone", "_destination_trigger_zone"):
        if key + suffix in zones:
            return zones[key + suffix], key + suffix
    raise SystemExit("no zone for " + key)


def flat(a, b):
    return math.hypot(a[0] - b[0], a[2] - b[2])


def main():
    zones = read_zones(sys.argv[1])
    sites = []
    for key, shown, kind in SITES:
        pos, source = site_position(zones, key)
        sites.append((key, shown, kind, pos, source))

    # race starts on the mainland (Storm Island and stray zones have z > 0)
    starts = [(n, p) for n, p in zones.items() if n.startswith("race_trigger_zone_rt") and p[2] < 0]
    starts.sort(key=lambda item: int(item[0][len("race_trigger_zone_rt"):]))

    places = []  # (shown name, kind, pos, source)
    for key, shown, kind, pos, source in sites:
        places.append((shown, kind, pos, source, key))
    counters = {}
    for name, pos in starts:
        near = min(sites, key=lambda s: flat(s[3], pos))
        counters[near[1]] = counters.get(near[1], 0) + 1
        far = flat(near[3], pos)
        label = ("%s · Treffpunkt %d" if far < 2500 else "Landstraße bei %s · %d") % (
            near[1], counters[near[1]])
        places.append((label, 2, pos, name, near[0]))

    index = {p[3]: i for i, p in enumerate(places)}
    site_index = {key: index[source] for key, _, _, _, source in sites}

    lines = []
    for number, title, keys in LINES:
        stops = []
        for a, b in zip(keys, keys[1:]):
            stops.append(site_index[a])
            pa, pb = places[site_index[a]][2], places[site_index[b]][2]
            used = set()
            for t in (1 / 3, 2 / 3):
                mid = (pa[0] + (pb[0] - pa[0]) * t, 0, pa[2] + (pb[2] - pa[2]) * t)
                best = None
                for i, p in enumerate(places):
                    if p[1] != 2 or i in used:
                        continue
                    d = flat(p[2], mid)
                    if d < 900 and (best is None or d < best[0]):
                        best = (d, i)
                if best:
                    used.add(best[1])
                    stops.append(best[1])
        stops.append(site_index[keys[-1]])
        lines.append((number, title, stops))
    for number, title, town, count in TOWN_LINES:
        lines.append((number, title, town_line([(p[0], p[1], p[2]) for p in places], town, count)))

    out = []
    out.append("/* Generated by tools/gen_stops.py from the game's triggerzones (tz_race_activations/triggers.tz).")
    out.append("   Do not edit by hand. Every position is a trigger zone of the game; see the tool for the names. */")
    out.append('#include "stops.h"')
    out.append("")
    out.append("const TsPlace ts_places[] = {")
    for shown, kind, pos, source, key in places:
        out.append('    {"%s", %d, {%.2ff, %.2ff, %.2ff}}, /* %s */' % (shown, kind, pos[0], pos[1], pos[2], source))
    out.append("};")
    out.append("const unsigned ts_place_count = sizeof(ts_places) / sizeof(ts_places[0]);")
    out.append("")
    for number, title, stops in lines:
        out.append("static const unsigned short line_%s[] = {%s};" % (number, ", ".join(str(s) for s in stops)))
    out.append("const TsLine ts_lines[] = {")
    for number, title, stops in lines:
        out.append('    {"%s", "%s", line_%s, %d},' % (number, title, number, len(stops)))
    out.append("};")
    out.append("const unsigned ts_line_count = sizeof(ts_lines) / sizeof(ts_lines[0]);")
    sys.stdout.buffer.write(("\n".join(out) + "\n").encode("utf-8"))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Generates src/core/stops_data.c from the game's own files.

Every position in the generated table comes from Forza Horizon 2's files, nothing is invented
(all paths below media/tracks/anthem/):
  triggerzones/tz_race_activations/triggers.tz
    - garage_spatial_zone ................................. the main Horizon festival (only to find it)
    - <town>_destination_spatial_zone / _trigger_zone ..... the festival outposts
    - race_trigger_zone_rtNNNN ............................ race starts (they sit on roads)
  extras/Anthem.track (XML of the placed models)
    - fra_obj_sign_busstop ................................ the bus stop signs the map makers put up (0.8)
    - fra_obj_sign_freeway_* / *bridge_highway* ........... motorway markers: no race start near them is used
  trackroutes/ParkingAreas.nt
    - type*_parking_area_NNN .............................. the game's parking spots (0.8)
  triggerzones/tz_carmeets/car_meet_activation_triggers.tz
    - car_meet_ita_coast .................................. the car meet: nothing is placed near it

Usage:
  python tools/gen_stops.py <media/tracks/anthem folder> > src/core/stops_data.c

0.8 (realistic places): rides and buses use real places first: the bus stops, the parking spots, the festival and
the outposts. Race starts are only kept on the land (not in a town that has enough real places, not near the
festival grounds, the car meet or a motorway marker). The festival's stop is no longer the garage zone in the middle
of the grounds but the race start on the road through the festival.

Outpost names: the trigger zones use internal names (antrani, montichello, ...). Which in-game outpost
each one is was matched by the game's own road-trip cutscene names in the game database
(RoadTripLocations: ArriveAmalfi, ArriveMonti, ArriveSanGimi, ArriveGorbio, ArriveWNice, ArriveSisteron)
and the destination pictures (DestinationHubs: Casteletto, Montellino, San_Giovanni, Saint_Martin,
Nice_Massena, Sisteron). "alpesdehaute" has no road-trip entry and keeps a neutral name.
That match is a reading of the data, not confirmed in game: docs/MACHBARKEIT.md says so too.
"""
import math
import os
import re
import sys

# kinds (src/core/stops.h)
FESTIVAL, OUTPOST, SPOT, STOP, PARKING = 0, 1, 2, 3, 4

# internal zone name -> (shown name, kind)
SITES = [
    ("garage", "Horizon Festival", FESTIVAL),
    ("antrani", "Castelletto", OUTPOST),
    ("montichello", "Montellino", OUTPOST),
    ("sangimignano", "San Giovanni", OUTPOST),
    ("eastnice", "Saint Martin", OUTPOST),
    ("westnice", "Nizza", OUTPOST),
    ("sisteron", "Sisteron", OUTPOST),
    ("alpesdehaute", "Alpes de Haute", OUTPOST),
]
# the festival's stop: on the road through the festival, not in the garage zone (the grounds, the car meet area)
FESTIVAL_STOP = "race_trigger_zone_rt89"
FESTIVAL_GROUNDS_M = 450.0  # around the garage zone: the grounds, nothing else is placed there
CAR_MEET_M = 500.0          # around the car meet zone (its parking lot too)
MOTORWAY_M = 200.0          # around a motorway marker (overhead sign, motorway bridge)
TOWN_M = 2500.0             # a place this near an outpost is in its town (Nice is big)
TOWN_REAL_ENOUGH = 6        # a town with this many real places (stops, parking) needs no race starts
STOP_PAIR_M = 45.0          # stop signs on both sides of a street are one stop
PARKING_SPACING_M = 120.0   # parking spots nearer than this to one already taken are left out
NEAR_REAL_M = 300.0         # a race start this near a real place is left out (the real place is used)

# Bus lines: stop order by site key; legs get up to two stops in between, real places first.
LINES = [
    ("1", "Festival-Ring", ["garage", "sangimignano", "montichello", "antrani", "garage"]),
    ("2", "Küstenlinie", ["garage", "sangimignano", "eastnice", "westnice"]),
    ("3", "Bergexpress", ["eastnice", "alpesdehaute", "sisteron", "westnice", "eastnice"]),
]

# Town buses: a round from the town's outpost through its places (bus stops and parking first) and back.
TOWN_LINES = [
    ("4", "Nizza Stadtbus", "Nizza", 9),
    ("5", "Castelletto-Shuttle", "Castelletto", 8),
    ("6", "Weinberg-Linie", "Montellino", 8),
]


def flat(a, b):
    return math.hypot(a[0] - b[0], a[2] - b[2])


def read_zones(path):
    text = open(path, encoding="utf-8-sig").read()
    zones = {}
    for attrs, x, y, z in re.findall(
        r'<triggerzone([^>]*)>\s*<position x="([-\d.]+)" y="([-\d.]+)" z="([-\d.]+)"', text
    ):
        name = re.search(r'name="([^"]*)"', attrs).group(1)
        zones[name] = (float(x), float(y), float(z))
    return zones


def read_models(path):
    """(model file name, position) of every placed model in Anthem.track"""
    text = open(path, encoding="utf-8-sig", errors="replace").read()
    out = []
    for model, transform in re.findall(r'<Model value="([^"]*)" />\s*<Transform value="([^"]*)"', text):
        v = [float(t) for t in transform.split()]
        out.append((model.replace("\\", "/").split("/")[-1].lower(), (v[12], v[13], v[14])))
    return out


def read_parking(path):
    text = open(path, encoding="utf-8", errors="replace").read()
    out = []
    for name, x, y, z in re.findall(
        r"<Name value='([^']*)'/>.*?value\._41=\"([-\d.]+)\" value\._42=\"([-\d.]+)\" value\._43=\"([-\d.]+)\"", text, re.S
    ):
        out.append((name.strip(), (float(x), float(y), float(z))))
    return out


def site_position(zones, key):
    for suffix in ("_spatial_zone", "_destination_spatial_zone", "_destination_trigger_zone"):
        if key + suffix in zones:
            return zones[key + suffix], key + suffix
    raise SystemExit("no zone for " + key)


def on_mainland(p):
    return p[2] < 0 and abs(p[0]) + abs(p[2]) > 1.0  # Storm Island and stray models have z > 0 or sit at 0,0,0


def town_line(places, town, count):
    """places: [(shown name, kind, pos)]. Starts at the town's outpost, then the nearest place of the town at least
    300 m from every stop so far (160 m between two bus stops: they stand that close in town), a real bus stop if one is within 1500 m,
    else a real place within 1200 m, and back to the outpost."""
    home = next(i for i, p in enumerate(places) if p[0] == town and p[1] == OUTPOST)
    points = [i for i, p in enumerate(places) if p[1] >= SPOT and p[0].startswith(town + " · ")]
    stops = [home]
    while len(stops) < count - 1:
        last = places[stops[-1]][2]
        def gap(i, j):
            return 160.0 if places[i][1] == STOP and places[j][1] == STOP else 300.0

        free = [i for i in points if i not in stops and all(flat(places[i][2], places[j][2]) >= gap(i, j) for j in stops)]
        if not free:
            break
        signs = [i for i in free if places[i][1] == STOP and flat(places[i][2], last) < 1500.0]
        real = [i for i in free if places[i][1] != SPOT and flat(places[i][2], last) < 1200.0]
        stops.append(min(signs or real or free, key=lambda i: flat(places[i][2], last)))
    stops.append(home)
    return stops


def main():
    root = sys.argv[1]
    zones = read_zones(os.path.join(root, "triggerzones", "tz_race_activations", "triggers.tz"))
    meets = read_zones(os.path.join(root, "triggerzones", "tz_carmeets", "car_meet_activation_triggers.tz"))
    models = read_models(os.path.join(root, "extras", "Anthem.track"))
    parking = read_parking(os.path.join(root, "trackroutes", "ParkingAreas.nt"))

    sites = []
    for key, shown, kind in SITES:
        pos, source = site_position(zones, key)
        sites.append((key, shown, kind, pos, source))
    grounds = sites[0][3]
    meet_pos = list(meets.values())
    motorway = [p for m, p in models if on_mainland(p) and re.search(r"sign_freeway|bridge_highway|bridge_railbridge_highway", m)]

    def nearest_site(p):
        return min(sites, key=lambda s: flat(s[3], p))

    def blocked(p):
        return flat(p, grounds) < FESTIVAL_GROUNDS_M or any(flat(p, m) < CAR_MEET_M for m in meet_pos)

    places = []  # (shown name, kind, pos, source)
    for key, shown, kind, pos, source in sites:
        if kind == FESTIVAL:
            pos, source = zones[FESTIVAL_STOP], FESTIVAL_STOP + " (the road through the festival)"
        places.append((shown, kind, pos, source))
    counters = {}

    def label(p, what):
        near = nearest_site(p)
        counters[(near[1], what)] = counters.get((near[1], what), 0) + 1
        n = counters[(near[1], what)]
        if flat(near[3], p) < 2500:
            return "%s · %s %d" % (near[1], what, n)
        return "Bei %s · %s %d" % (near[1], what, n)

    # the bus stop signs (both sides of a street: one stop in the middle)
    signs = [p for m, p in models if m == "fra_obj_sign_busstop.max" and on_mainland(p)]
    signs.sort(key=lambda p: (p[0], p[2]))
    used = set()
    for i, p in enumerate(signs):
        if i in used:
            continue
        group = [p] + [q for j, q in enumerate(signs) if j > i and j not in used and flat(p, q) < STOP_PAIR_M]
        used.update(j for j, q in enumerate(signs) if q in group)
        mid = tuple(sum(q[k] for q in group) / len(group) for k in range(3))
        if blocked(mid):
            continue
        places.append((label(mid, "Haltestelle"), STOP, mid, "fra_obj_sign_busstop x%d" % len(group)))

    # the parking spots
    taken = []
    for name, p in sorted(parking, key=lambda item: item[0]):
        if not on_mainland(p) or blocked(p) or any(flat(p, q) < PARKING_SPACING_M for q in taken):
            continue
        if any(flat(p, q[2]) < PARKING_SPACING_M for q in places):
            continue
        taken.append(p)
        places.append((label(p, "Parkplatz"), PARKING, p, name))

    # how many real places each town has
    real_in_town = {}
    for shown, kind, pos, source in places:
        if kind in (STOP, PARKING):
            near = nearest_site(pos)
            if flat(near[3], pos) < TOWN_M:
                real_in_town[near[1]] = real_in_town.get(near[1], 0) + 1

    # race starts on the land (Storm Island and stray zones have z > 0)
    starts = [(n, p) for n, p in zones.items() if n.startswith("race_trigger_zone_rt") and p[2] < 0 and n != FESTIVAL_STOP]
    starts.sort(key=lambda item: int(item[0][len("race_trigger_zone_rt"):]))
    for name, pos in starts:
        if blocked(pos) or any(flat(pos, m) < MOTORWAY_M for m in motorway):
            continue
        if any(flat(pos, q[2]) < NEAR_REAL_M for q in places):
            continue
        near = nearest_site(pos)
        far = flat(near[3], pos)
        if far < TOWN_M and real_in_town.get(near[1], 0) >= TOWN_REAL_ENOUGH:
            continue
        counters[(near[1], "spot")] = counters.get((near[1], "spot"), 0) + 1
        n = counters[(near[1], "spot")]
        shown = ("%s · Treffpunkt %d" if far < 2500 else "Landstraße bei %s · %d") % (near[1], n)
        places.append((shown, SPOT, pos, name))

    site_index = {key: i for i, (key, _, _, _, _) in enumerate(sites)}

    lines = []
    for number, title, keys in LINES:
        stops = []
        for a, b in zip(keys, keys[1:]):
            stops.append(site_index[a])
            pa, pb = places[site_index[a]][2], places[site_index[b]][2]
            chosen = set()
            for t in (1 / 3, 2 / 3):
                mid = (pa[0] + (pb[0] - pa[0]) * t, 0, pa[2] + (pb[2] - pa[2]) * t)
                best = None
                for i, p in enumerate(places):
                    if p[1] < SPOT or i in chosen or i in stops or min(flat(p[2], pa), flat(p[2], pb)) < 300.0:
                        continue
                    d = flat(p[2], mid) + (300.0 if p[1] == SPOT else 0.0)  # a real place first
                    if d < 900 + (300.0 if p[1] == SPOT else 0.0) and (best is None or d < best[0]):
                        best = (d, i)
                if best:
                    chosen.add(best[1])
                    stops.append(best[1])
        stops.append(site_index[keys[-1]])
        lines.append((number, title, stops))
    for number, title, town, count in TOWN_LINES:
        lines.append((number, title, town_line([(p[0], p[1], p[2]) for p in places], town, count)))

    out = []
    out.append("/* Generated by tools/gen_stops.py from the game's files (triggers.tz, Anthem.track, ParkingAreas.nt).")
    out.append("   Do not edit by hand. Every position is from the game; see the tool for where each one comes from. */")
    out.append('#include "stops.h"')
    out.append("")
    out.append("const TsPlace ts_places[] = {")
    for shown, kind, pos, source in places:
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
    kinds = {}
    for p in places:
        kinds[p[1]] = kinds.get(p[1], 0) + 1
    sys.stderr.write("places: %d (kinds %s), real per town %s\n" % (len(places), kinds, real_in_town))


if __name__ == "__main__":
    main()

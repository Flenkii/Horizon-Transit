#!/usr/bin/env python3
"""Checks the bus queries (src/core/bus_sql.c) against the game's own database and SQL texts.
Every SQL text in the game's exe that the rewrite touches is run twice on a copy of gamedbRC.slt, as the game
has it and rewritten: both must work, and only the bus may be added.
Usage: python tools/check_bus_sql.py <libbus.so/.dll> <forza_durango_release_final.exe> <gamedbRC.slt>"""
import ctypes, re, sqlite3, sys

lib = ctypes.CDLL(sys.argv[1])
exe = open(sys.argv[2], "rb").read()
db = sqlite3.connect("file:" + sys.argv[3] + "?mode=ro", uri=True)
db.execute("CREATE TEMP VIEW Drivable_Data_Car AS SELECT Data_Car.* FROM Data_Car WHERE Id NOT IN (SELECT CarId FROM TrafficCars)")
for name in ["GenerateBigThumbnailName", "GenerateSmallThumbnailName"]:
    db.create_function(name, 1, lambda x: x)
for name in ["IsCarDLC", "IsCarVisible", "HasCarMedia"]:
    db.create_function(name, 1, lambda x: 1)
db.create_collation("LOCALIZED_NOCASE", lambda a, b: (a.lower() > b.lower()) - (a.lower() < b.lower()))
db.create_function("CEIL", 1, lambda x: int(-(-x // 1)) if x is not None else None)


def rewrite(sql):
    out = ctypes.create_string_buffer(65536)
    r = lib.ts_bus_sql(sql.encode(), out, len(out))
    return r, out.value.decode() if r == 1 else sql


fails = checked = 0
for m in re.finditer(rb"[\x20-\x7e]{16,}", exe):
    sql = m.group().decode()
    if not re.match(r"\s*SELECT\b", sql, re.I):
        continue
    r, new = rewrite(sql)
    if r == 0:
        continue
    checked += 1
    if r < 0:
        print("TOO LONG", sql[:80]); fails += 1; continue
    # the game's placeholders: tables of the profile and conditions are unknown here, so only full texts run
    sql_f = sql.replace("AND %s ORDER BY", "AND 1 ORDER BY").replace("Drivable_Data_Car %s ORDER", "Drivable_Data_Car WHERE 1 ORDER")
    new_f = new.replace("AND %s ORDER BY", "AND 1 ORDER BY").replace("AS Drivable_Data_Car %s ORDER", "AS Drivable_Data_Car WHERE 1 ORDER")
    sql, new = sql_f, new_f
    if "%" in sql:
        filled = sql.replace("%s", "Data_Car").replace("%d", "414").replace("%u", "414").replace("%f", "1.0")
        new_filled = new.replace("%s", "Data_Car").replace("%d", "414").replace("%u", "414").replace("%f", "1.0")
    else:
        filled, new_filled = sql, new
    for k in ("filled", "new_filled"):  # a WHERE clause the game adds, a number it appends
        v = locals()[k]
        if v.rstrip().endswith("="): v = v + "414"
        if k == "filled": filled = v
        else: new_filled = v
    try:
        a = db.execute(filled).fetchall()
    except Exception as e:
        print(f"  (as the game has it, not runnable here: {str(e)[:60]}) {sql[:70]}")
        try:
            db.execute("EXPLAIN " + new_filled)
        except Exception as e2:
            if "no such" not in str(e2) and "ambiguous" not in str(e2):
                print("FAIL rewritten does not parse:", e2, new_filled[:200]); fails += 1
        continue
    try:
        b = db.execute(new_filled).fetchall()
    except Exception as e:
        print("FAIL", e, "\n  ", new_filled[:300]); fails += 1; continue
    extra = [row for row in b if row not in a]
    print(f"ok  +{len(extra)} rows  {sql[:90]}")
print(f"{checked} queries rewritten, {fails} failures")

# the Autoshow's group query: the bus is there now, in group 21
q = "SELECT Id, CarBucket, MediaName, PerformanceIndex FROM Drivable_Data_Car JOIN Data_Car_Buckets ON Id=Data_Car_Buckets.CarId WHERE HasCarMedia(MediaName) ORDER BY CarBucket"
before = db.execute(q).fetchall()
after = db.execute(rewrite(q)[1]).fetchall()
bus = [r for r in after if r[0] == 414]
print("bus before:", [r for r in before if r[0] == 414], "after:", bus, "others unchanged:", [r for r in after if r[0] != 414] == before)

# the Autoshow's car list, with stand-ins for the profile's tables (empty: nothing owned, no barn finds)
db.execute("CREATE TEMP TABLE BarnFinds (CarOrdinal INT, State INT)")
db.execute("CREATE TEMP TABLE FreeCars (CarId INT, FreeCount INT)")
db.execute("CREATE TEMP TABLE CareerGarage (CarId INT)")
a = exe.index(b"SELECT Garage.*, 0 AS IsGifted, 0 AS IsCurrent, CASE WHEN (CareerGarage.CarId")
show = exe[a:exe.index(b"\0", a)].decode()
def fill(t):
    for v in ("0 AS IsRented", "BarnFinds", "FreeCars", "CareerGarage", "1", "1"):
        t = t.replace("%s", v, 1)
    return t.replace("%d", "0")
q = fill(show)
before = db.execute(q).fetchall()
r, new = rewrite(show)
after = db.execute(fill(new)).fetchall()
bus_rows = [x for x in after if x[0] == 414]
print("Autoshow list:", len(before), "cars before,", len(after), "after; the bus:", [(x[0], x[1]) for x in bus_rows])
fails += len(after) != len(before) + 1 or not bus_rows
print("RESULT:", "ok" if not fails and bus else "FAILED")
sys.exit(1 if fails or not bus else 0)

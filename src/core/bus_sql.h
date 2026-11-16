#ifndef TS_BUS_SQL_H
#define TS_BUS_SQL_H
/* EXPERIMENTAL: the game's traffic bus (PG_BUS_14, Data_Car 414) in the Autoshow, without changing the
   database file (so it also works when the game is started from the FH2MP launcher, where the platform
   leaves the database alone).

   The game asks its database with SQL text. The Autoshow lists Drivable_Data_Car, a view of Data_Car
   without the cars in TrafficCars, joined with Data_Car_Buckets (the Autoshow's groups). The bus is in
   TrafficCars and has no group. The mod's hook on the game's query function rewrites only those
   queries, as they are asked:
     - Drivable_Data_Car  -> the same view, but the bus counts as drivable
     - Data_Car_Buckets   -> the table plus one row: the bus in group 21 (the Ford Transit SSV's)
     - "FROM TrafficCars WHERE CarId=" (is this a traffic car?) -> not for the bus
   Everything else goes through unchanged. Pure C, tested in tests/test_core.c and against a copy of
   the game's database (tools/check_bus_sql.py). */
#include <stddef.h>

#define TS_BUS_CAR_ID 414
#define TS_BUS_BUCKET 21

/* Rewrites `sql` into `out` (cap bytes). Returns 1 if it changed something, 0 if the query stays as it is
   (out is then untouched), -1 if out was too small (use the original). */
int ts_bus_sql(const char *sql, char *out, size_t cap);

#endif

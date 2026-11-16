#ifndef TS_STOPS_H
#define TS_STOPS_H
/* Places in the game world, all taken from the game's own trigger zones (tools/gen_stops.py). */

enum { TS_PLACE_FESTIVAL = 0, TS_PLACE_OUTPOST = 1, TS_PLACE_SPOT = 2 };

typedef struct {
    const char *name; /* UTF-8, shown to the player */
    int kind;         /* TS_PLACE_* */
    float pos[3];     /* world position (x, y = height, z) */
} TsPlace;

typedef struct {
    const char *number, *title;
    const unsigned short *stops; /* indices into ts_places, in driving order */
    unsigned stop_count;
} TsLine;

extern const TsPlace ts_places[];
extern const unsigned ts_place_count;
extern const TsLine ts_lines[];
extern const unsigned ts_line_count;

#endif

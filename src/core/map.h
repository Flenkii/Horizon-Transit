#ifndef TS_MAP_H
#define TS_MAP_H
/* 0.7: the phone's map. A picture of the whole game map (made by tools/make_map.py from a picture the player has:
   it isn't part of the mod), read from an 8-bit BMP: 256 colours, a colour's 4th byte = 1 marks a road.
   The picture covers a fixed rectangle of the world (TS_MAP_X0..). Plain C, no Windows. */
#include <stddef.h>
#include <stdint.h>

/* the world rectangle a BMP picture covers (0.9: the game's picture brings its own): x from left to right, z from
   top (north) to bottom */
#define TS_MAP_X0 (-7500.0f)
#define TS_MAP_X1 (13500.0f)
#define TS_MAP_Z0 (470.0f)
#define TS_MAP_Z1 (-9195.0f)

/* 0.9: the roads' kinds (gamemap.c) */
enum { TS_ROAD_TRAIL = 0, TS_ROAD_DIRT = 1, TS_ROAD_STREET = 2, TS_ROAD_MOTORWAY = 3 };

/* 0.9: the picture as colours (w*h 0xRRGGBB, top row first) with a flag per pixel (1 road, 2 water; flags may be
   NULL), and the roads as lines: n segments (x0, z0, x1, z1) with a TS_ROAD_* kind each (seg may be NULL). All
   copied; the map is there (ts_map_ok) only when all is done, so it can be made on another thread while the phone
   draws. The roads are drawn sharp when zoomed in; the ways follow the streets and dirt roads. rect: the world the
   picture covers (x0, x1, z0 top, z1 bottom), NULL = TS_MAP_*. plain (may be NULL): the picture without the roads
   drawn in, for the nearest zoom (where the roads are drawn as lines). 1 = ok */
int ts_map_load_rgb(int w, int h, const uint32_t *rgb, const uint32_t *plain, const uint8_t *flags, const float *seg, const uint8_t *kind, int n, const float *rect);
int ts_map_road_count(void);

/* reads the picture from a BMP file's bytes (8 bit, uncompressed); 1 = ok. Keeps its own copy; again = replaces it. */
int ts_map_load_bmp(const void *data, size_t size);
void ts_map_free(void);
int ts_map_ok(void);
int ts_map_width(void);
int ts_map_height(void);

/* the map's colour (0xRRGGBB) at the world position x/z, for a screen pixel `mpp` metres wide (picks a smaller
   level of the picture when zoomed out, so roads don't flicker) */
uint32_t ts_map_color(float x, float z, float mpp);

/* the way along the map's roads from a to b (world x/z): writes up to max points (x, z pairs) into out and returns
   how many (a and b included); 0 = no map. Where the roads have gaps the way crosses the land a short bit.
   Remembers the last ways (a bus line asks for the same legs again). */
int ts_map_route(const float a[2], const float b[2], float *out, int max);

#endif

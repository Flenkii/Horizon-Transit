#ifndef TS_GAMEMAP_H
#define TS_GAMEMAP_H
/* 0.9: the phone's map from the game's own files, read on the player's PC (nothing of the game is in the mod):
   - media\ui\textures\data_bound\Anthem_Map.zip: the pause menu's map picture (low-0-0.swatchbin: BC7, in the
     Xbox One's tiled order); restyled as a dark phone map (land, forest, fields, towns, water)
   - media\tracks\anthem\ribbon_00\anthem_00.nav: the game's road network (the AI's lanes: streets, dirt roads,
     trails); drawn on top and used for the ways
   The picture's place in the world is the pause menu's own (media\ui\MapProfilePauseMenu.xml: 12000 m high from
   z -11000, x from -7500, 21000 m wide); the roads lie on it to the metre. Plain C, no Windows. */
#include <stddef.h>
#include <stdint.h>

enum { TS_GAMEMAP_PICTURE = 1, TS_GAMEMAP_ROADS = 2 };
/* 0.13: bumped when the map pieces look different (2: the nav's road types 5 and 6 are streets) */
#define TS_MAP_VERSION 2

/* builds the map (map.h) from the files' bytes; either may be NULL. Returns TS_GAMEMAP_* bits for what worked
   (0: nothing, the map stays as it was). */
int ts_gamemap_load(const uint8_t *zip, size_t zip_size, const uint8_t *nav, size_t nav_size);

/* the pieces, for the tests */
/* the block index of block (bx, by) in the Xbox One's tiled order (GCN 2D thin, 8 pipes, 128 bits a block) for a
   texture blocks_w blocks wide (a multiple of 64) */
uint32_t ts_gamemap_tile_index(uint32_t bx, uint32_t by, uint32_t blocks_w);
/* the roads in anthem_00.nav: count of segments written (x0, z0, x1, z1 into seg; TS_ROAD_* into kind), at most
   max; 0 = not a nav file */
int ts_gamemap_roads(const uint8_t *nav, size_t nav_size, float *seg, uint8_t *kind, int max);

#endif

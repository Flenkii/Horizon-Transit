#ifndef TS_BC7_H
#define TS_BC7_H
/* 0.9: decodes one BC7 block (16 bytes) into 4x4 RGBA pixels (row by row, 4 bytes each). For the game's map. */
#include <stdint.h>
void ts_bc7_block(const uint8_t *src, uint8_t out[64]);
#endif

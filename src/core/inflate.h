#ifndef TS_INFLATE_H
#define TS_INFLATE_H
/* 0.9: unpacks raw deflate data (what a zip's method 8 holds), for reading the game's map from its own files.
   Plain C, no Windows. */
#include <stddef.h>
#include <stdint.h>

/* unpacks src into dst (exactly dst_size bytes expected); 1 = ok and the size came out right */
int ts_inflate(const uint8_t *src, size_t src_size, uint8_t *dst, size_t dst_size);

/* finds a file in a zip's bytes (by its exact name) and unpacks it (stored or deflate): a new malloc'd buffer and
   its size, or NULL */
uint8_t *ts_unzip(const uint8_t *zip, size_t zip_size, const char *name, size_t *out_size);

#endif

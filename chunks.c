#include "chunks.h"
#include "noise.h"

NOISE(2);

#define CHUNK_Y 5
#define CHUNK_SIZE (1 << CHUNK_Y)
#define CHUNK_M (CHUNK_SIZE * CHUNK_SIZE)

int chunk_load() {
	static octave_t oct[] = \
		{{ 7, 2 }, { 6, 2 }, { 5, 2 }, { 4, 3 }, { 3, 3 }, { 2, 3 }};

	noise_t m[CHUNK_M];
	int16_t s[2] = { 0, 0 };

	noise_oct2(m, s, sizeof(oct) / sizeof(octave_t), oct, 0, CHUNK_M, CHUNK_Y);
	return 0;
}

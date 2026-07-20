#include <stdlib.h>

// TODO FINAL grow amap
#define STATIC_RAM_SIZE (90 * 1024)
#define MAX_CHUNKS 10

extern size_t static_ram_used;

typedef struct {
	uint8_t *ptr;
	size_t len;
} Slice;

typedef struct {
	void *ptr;
	size_t size;
	size_t used;
} Chunk;

// TODO CONSIDER REMOVE
extern Chunk chunks[MAX_CHUNKS];
extern int chunk_count;

uint8_t *static_alloc(size_t size);

bool static_free(size_t size);

void init_mem();

#include "util.h"

static uint8_t static_ram[STATIC_RAM_SIZE];
size_t static_ram_used;

uint8_t *static_alloc(size_t size) {
	const bool space_left = static_ram_used + size <= STATIC_RAM_SIZE;
	uint8_t *const result =
		(uint8_t *) (space_left * ((uintptr_t) (static_ram + static_ram_used)));
	static_ram_used += space_left * size;
	return result;
}

bool static_free(size_t size) {
	const bool space_used = size <= static_ram_used;
	static_ram_used = space_used * (static_ram_used - size);
	return space_used;
}

// void init_mem() {
// 	const size_t leave_size = 8192; // NOTE arbitrary
// 	const size_t remaining = esp_get_free_heap_size();
// 	const size_t target = (remaining > leave_size) ? remaining - leave_size : 0;
// 
// 	size_t heap_grabbed = 0;
// 
// 	while (chunk_count < MAX_CHUNKS && heap_grabbed < target) {
// 		const size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
// 		if (largest == 0) break;
// 
// 		const size_t take = (heap_grabbed + largest > target) ? target - heap_grabbed : largest;
// 		void *ptr = malloc(take);
// 		if (!ptr) break;
// 
// 		chunks[chunk_count++] = (Chunk) {
// 			.ptr = ptr,
// 			.size = take,
// 			.used = 0
// 		};
// 		heap_grabbed += take;
// 	}
// 
// 	printf("=== Init ===\n");
// 	printf("Static:         %u kB\n", STATIC_RAM_SIZE / 1024);
// 	printf("Heap:           %zu kB in %d chunks\n", heap_grabbed / 1024, chunk_count);
// 	printf("Total grabbed:  %zu kB\n", (STATIC_RAM_SIZE + heap_grabbed) / 1024);
// 	printf("Free remaining: %lu B\n", esp_get_free_heap_size());
// 	// ^ NOTE Ignore the LSP message lol
// }


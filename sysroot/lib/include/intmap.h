#include "map.h"
#include "stdint.h"

int intmap_add(const struct Map *map, uintptr_t key, uintptr_t value);
int intmap_remove(const struct Map *map, uintptr_t key);
void* intmap_get(const struct Map *map, uintptr_t key);
int intmap_contains(const struct Map *map, uintptr_t key);
void intmap_free(struct Map *map, void (*freeValue)(uintptr_t value));

int intmap_comparitor(void *key1, void *key2);

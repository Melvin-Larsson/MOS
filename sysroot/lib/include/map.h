#ifndef MAP_H_INCLUDED
#define MAP_H_INCLUDED

#include "stdint.h"

typedef struct Map{
    void *data;
    int (*comparitor)(void *key1, void *key2);

    int (*add)(const struct Map *map, void* key, void* value);
    int (*remove)(const struct Map *map, void* key);
    void*(*get)(const struct Map *map, void* key);
    int (*contains)(const struct Map *map, void* key);
    void (*free)(struct Map *map, void (*freeValue)(void *value));

    int (*validate)(const struct Map *map);
}Map;

Map *map_newBinaryMap(int (*comparitor)(void *key1, void *key2));

int map_add(const struct Map *map, void *key, void* value);
int map_remove(const struct Map *map, void *key);
void* map_get(const struct Map *map, void *key);
int map_contains(const struct Map *map, void *key);
void map_free(struct Map *map, void (*freeValue)(void *value));

int map_validate(const struct Map* map);

int map_ptrComparitor(void *key1, void *key2);


#endif

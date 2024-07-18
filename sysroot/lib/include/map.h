#ifndef MAP_H_INCLUDED
#define MAP_H_INCLUDED

typedef struct Map{
    void *data;
    int (*add)(const struct Map *map, int key, void* value);
    int (*remove)(const struct Map *map, int key);
    void*(*get)(const struct Map *map, int key);
    int (*contains)(const struct Map *map, int key);
    void (*free)(struct Map *map, void (*freeValue)(void *value));

    int (*validate)(const struct Map *map);
}Map;

Map *map_newBinaryMap();

int map_add(const struct Map *map, int key, void* value);
int map_remove(const struct Map *map, int key);
void* map_get(const struct Map *map, int key);
int map_contains(const struct Map *map, int key);
void map_free(struct Map *map, void (*freeValue)(void *value));

int map_validate(const struct Map* map);


#endif

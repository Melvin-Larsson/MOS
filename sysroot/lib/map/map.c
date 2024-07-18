#include "map.h"

int map_add(const struct Map *map, void* key, void* value){
    return map->add(map, key, value);
}
int map_remove(const struct Map *map, void* key){
   return map->remove(map, key);
}
void* map_get(const struct Map *map, void* key){
    return map->get(map, key);
}
int map_contains(const struct Map *map, void *key){
    return map->contains(map, key);
}
void map_free(struct Map *map, void (*freeValue)(void *value)){
    map->free(map, freeValue);
}

int map_validate(const struct Map* map){
    return map->validate(map);
}

int map_ptrComparitor(void *key1, void *key2){
    if(key1 > key2){
        return 1;
    }
    if(key1 < key2){
        return -1;
    }
    return 0;
}

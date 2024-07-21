#include "intmap.h"

int intmap_add(const struct Map *map, uintptr_t key, uintptr_t value){
   return map->add(map, (void*)key, (void*)value); 
}
int intmap_remove(const struct Map *map, uintptr_t key){
   return map->remove(map, (void*)key); 
}
void* intmap_get(const struct Map *map, uintptr_t key){
   return map->get(map, (void*)key); 
}
int intmap_contains(const struct Map *map, uintptr_t key){
   return map->contains(map, (void*)key); 
}

int intmap_comparitor(void *key1, void *key2){
    if(key1 > key2){
        return 1;
    }
    if(key1 < key2){
        return -1;
    }
    return 0;
}

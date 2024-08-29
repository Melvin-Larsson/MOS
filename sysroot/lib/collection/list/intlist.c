#include "collection/list.h"

void intlist_add(const struct List *list, uintptr_t val){
    list->add(list, (void*)val);
}

bool intlist_removeAt(const struct List *list, unsigned int index){
    return list->removeAt(list, index);
}

bool intlist_remove(const struct List *list, uintptr_t val){
    return list->remove(list, (void*)val);
}

int intlist_length(const struct List *list){
    return list->length(list);
}

uintptr_t intlist_get(const struct List *list, unsigned int index){
    return (uintptr_t)list->get(list, index);
}

bool intlist_contains(const struct List *list, uintptr_t val){
    return list->contains(list, (void*)val);
}

void intlist_freeList(struct List *list){
    list->free(list);
}

Iterator *intlist_createIterator(struct List *list){
    return list->createIterator(list);
}

bool intlist_equals(void *val1, void *val2){
    return val1 == val2;
}

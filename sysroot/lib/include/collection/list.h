#ifndef LIST_H_INCLUDED
#define LIST_H_INCLUDED

#include "collection/iterator.h"
#include "stdbool.h"
#include "stdint.h"

typedef struct List{
    void *data;

    void (*add)(const struct List *list, void *val);
    bool (*removeAt)(const struct List *list, unsigned int index);
    bool (*remove)(const struct List *list, void *val);
    int (*length)(const struct List *list);
    void* (*get)(const struct List *list, unsigned int index);
    bool (*contains)(const struct List *list, void* entry);
    void (*free)(struct List *list);

    Iterator *(*createIterator)(struct List *list);
}List;

List *list_newLinkedList(bool (*equals)(void *val1, void *val2));

bool list_pointerEquals(void *val1, void *val2);

#endif

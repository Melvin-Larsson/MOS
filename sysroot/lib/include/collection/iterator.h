#ifndef ITERATOR_H_INCLUDED
#define ITERATOR_H_INCLUDED

#include "stdbool.h"

typedef struct Iterator{
    void *data;
    void *(*get)(const struct Iterator *iterator);
    void (*free)(struct Iterator *iterator);
    bool (*add)(const struct Iterator *iterator, void *value);
    bool (*remove)(const struct Iterator *iterator);
    bool (*hasNext)(const struct Iterator *iterator);
    bool (*advance)(const struct Iterator *iterator);
}Iterator;

#endif

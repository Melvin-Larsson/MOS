#ifndef INT_ITERATOR_H_INCLUDED
#define INT_ITERATOR_H_INCLUDED

#include "stdint.h"
#include "collection/iterator.h"

uintptr_t intIterator_get(const Iterator *iterator);
void intIterator_free(Iterator *iterator);
bool intIterator_addAfter(const Iterator *iterator, uintptr_t value);
bool intIterator_addAt(const Iterator *iterator, uintptr_t value);
bool intIterator_remove(const Iterator *iterator);
bool intIterator_hasNext(const Iterator *iterator);
bool intIterator_advance(const struct Iterator *iterator);

#endif

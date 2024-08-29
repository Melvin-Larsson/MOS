#ifndef INTLIST_H_INCLUDED
#define INTLIST_H_INCLUDED

#include "stdint.h"
#include "list.h"

void intlist_add(const struct List *list, uintptr_t val);
bool intlist_removeAt(const struct List *list, int index);
bool intlist_remove(const struct List *list, uintptr_t val);
int intlist_length(const struct List *list);
uintptr_t intlist_get(const struct List *list, int index);
bool intlist_contains(const struct List *list, uintptr_t val);
void intlist_freeList(struct List *list);

Iterator *intlist_createIterator(struct List *list);

bool intlist_equals(void *val1, void *val2);

#endif

#include "collection/int-iterator.h"

uintptr_t intIterator_get(const Iterator *iterator){
    return (uintptr_t)iterator->get(iterator);
}
void intIterator_free(Iterator *iterator){
    iterator->free(iterator);
}
bool intIterator_addAfter(const Iterator *iterator, uintptr_t value){
    return iterator->addAfter(iterator, (void*)value);
}
bool intIterator_addAt(const Iterator *iterator, uintptr_t value){
    return iterator->addAt(iterator, (void*)value);
}
bool intIterator_remove(const Iterator *iterator){
    return iterator->remove(iterator);
}
bool intIterator_hasNext(const Iterator *iterator){
    return iterator->hasNext(iterator);
}
bool intIterator_advance(const struct Iterator *iterator){
    return iterator->advance(iterator);
}

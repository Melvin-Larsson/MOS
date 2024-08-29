#include "collection/list.h"
#include "stdlib.h"

typedef struct ListNode{
    struct ListNode *next;
    void *value;
}ListNode;

typedef struct{
    ListNode *firstNode;
    ListNode *lastNode;
    unsigned int length;
    bool (*equals)(void *, void *);
    unsigned int modifications;
}LinkedList;

typedef struct{
    ListNode *node;
    ListNode *last;
    ListNode *dummyNode;
    LinkedList *list;
    unsigned int modificationsOnCreation;
}ListIterator;

static void add(const struct List *list, void *val);
static bool removeAt(const struct List *list, unsigned int index);
static bool remove(const struct List *list, void *val);
static int length(const struct List *list);
static void* get(const struct List *list, unsigned int index);
static bool contains(const struct List *list, void* entry);
static void freeList(struct List *list);

Iterator *createIterator(struct List *list);

static void* iterator_get(const Iterator *iterator);
static void iterator_free(Iterator *iterator);
static bool iterator_addAfter(const Iterator *iterator, void *value);
static bool iterator_addAt(const Iterator *iterator, void *value);
static bool iterator_remove(const Iterator *iterator);
static bool iterator_hasNext(const Iterator *iterator);
static bool iterator_advance(const Iterator *iterator);

List *list_newLinkedList(bool (*equals)(void *val1, void *val2)){
    LinkedList *list = malloc(sizeof(LinkedList));
    *list = (LinkedList){
        .firstNode = 0,
        .lastNode = 0,
        .length = 0,
        .equals = equals,
        .modifications = 0,
    };
    List *result = malloc(sizeof(List));
    *result = (List){
        .data = list,
        .add = add,
        .removeAt = removeAt,
        .remove = remove,
        .length = length,
        .get = get,
        .contains = contains,
        .free = freeList,
        .createIterator = createIterator
    };
    return result;
}

static ListNode *createNewNode(void *value){
    ListNode *newNode = malloc(sizeof(ListNode));
    *newNode = (ListNode){
        .value = value,
        .next = 0,
    };
    return newNode;
}

static void add(const struct List *list, void *value){
    LinkedList *linkedList = list->data;
    linkedList->length++;

    ListNode *newNode = createNewNode(value);

    if(linkedList->lastNode == 0){
        linkedList->lastNode = newNode;
        linkedList->firstNode = newNode;
        return;
    }

    linkedList->lastNode->next = newNode;
    linkedList->lastNode = newNode;

    linkedList->modifications++;
}

static bool find(LinkedList *list, void *value, ListNode **prev, ListNode **result){
    *prev = 0;
    *result = list->firstNode;
    while(*result){
        if(list->equals((*result)->value, value)){
            return true;
        }
        *prev = *result;
        *result = (*result)->next;
    }
    return false;
}

static bool findAt(LinkedList *list, unsigned int index, ListNode **prev, ListNode **result){
    if(index >= list->length){
        return false;
    }

    *prev = 0;
    *result = list->firstNode;
    while(*result && index > 0){
        *prev = *result;
        *result = (*result)->next;
        index--;
    }
    return true;
}

static void removeNode(LinkedList *list, ListNode *prev, ListNode *toRemove){
    if(prev){
        prev->next = toRemove->next;
    }else{
        list->firstNode = toRemove->next;
    }
    if(toRemove->next == 0){
        list->lastNode = prev;
    }

    list->length--;
    free(toRemove);
}

static bool removeAt(const struct List *list, unsigned int index){
    LinkedList *linkedList = list->data;

    ListNode *prev;
    ListNode *result;
    if(findAt(linkedList, index, &prev, &result)){
        removeNode(linkedList, prev, result);
        linkedList->modifications++;
        return true;
    }
    return false;
}

static bool remove(const struct List *list, void *val){
    LinkedList *linkedList = list->data;

    ListNode *prev;
    ListNode *result;
    if(find(linkedList, val, &prev, &result)){
        removeNode(linkedList, prev, result);
        linkedList->modifications++;
        return true;
    }

    return false;
}

static int length(const struct List *list){
    LinkedList *linkedList = list->data;
    return linkedList->length;
}

static void* get(const struct List *list, unsigned int index){
    LinkedList *linkedList = list->data;

    ListNode *prev;
    ListNode *result;
    if(findAt(linkedList, index, &prev, &result)){
        return result->value;
    }
    return 0;
}
static bool contains(const struct List *list, void* entry){
    LinkedList *linkedList = list->data;

    ListNode *prev;
    ListNode *result;
    if(find(linkedList, entry, &prev, &result)){
        return true;
    }
    return false;
}

static void freeList(struct List *list){
    LinkedList *linkedList = list->data;

    ListNode *node = linkedList->firstNode;
    while(node){
        ListNode *next = node->next;
        free(node);
        node = next;
    }

    free(linkedList);
    free(list);
}

Iterator *createIterator(struct List *list){
    LinkedList *linkedList = list->data;

    ListNode *dummyNode = malloc(sizeof(ListNode));
    *dummyNode = (ListNode){
        .next = linkedList->firstNode,
        .value = 0
    };

    ListIterator *listIterator = malloc(sizeof(ListIterator));
    *listIterator = (ListIterator){
        .dummyNode = dummyNode,
        .node = dummyNode,
        .last = 0,
        .list = linkedList,
        .modificationsOnCreation = linkedList->modifications
    };

    Iterator *iterator = malloc(sizeof(Iterator));
    *iterator = (Iterator){
        .data = listIterator,
        .get = iterator_get,
        .free = iterator_free,
        .addAfter = iterator_addAfter,
        .addAt = iterator_addAt,
        .remove = iterator_remove,
        .hasNext = iterator_hasNext,
        .advance = iterator_advance
    };

    return iterator;
}


static void* iterator_get(const Iterator *iterator){
    ListIterator *listIterator = iterator->data;

    if(listIterator->list->modifications != listIterator->modificationsOnCreation){
        return 0;
    }

    if(listIterator->node == listIterator->dummyNode){
        return 0;
    }

    return listIterator->node->value; 
}

static void iterator_free(Iterator *iterator){
    ListIterator *listIterator = iterator->data;
    free(listIterator->node);
    free(listIterator);
    free(iterator);
}

static bool iterator_addAfter(const Iterator *iterator, void *value){
    ListIterator *listIterator = iterator->data;
    if(listIterator->list->modifications != listIterator->modificationsOnCreation){
        return false;
    }

    ListNode *newNode = createNewNode(value);

    newNode->next = listIterator->node->next;
    listIterator->node->next = newNode;

    if(listIterator->list->lastNode == 0
            || listIterator->list->lastNode == listIterator->node){
        listIterator->list->lastNode = newNode;
    }

    listIterator->list->firstNode = listIterator->dummyNode->next;
    listIterator->list->length++;
    return true;
}

static bool iterator_addAt(const Iterator *iterator, void *value){
    ListIterator *listIterator = iterator->data;
    if(listIterator->list->modifications != listIterator->modificationsOnCreation){
        return false;
    }

    if(!listIterator->last){
        return false;
    }

    ListNode *newNode = createNewNode(value);

    newNode->next = listIterator->node;
    listIterator->last->next = newNode;

    listIterator->list->firstNode = listIterator->dummyNode->next;
    listIterator->list->length++;
    return true;
}

static bool iterator_remove(const Iterator *iterator){
    ListIterator *listIterator = iterator->data;
    if(listIterator->list->modifications != listIterator->modificationsOnCreation){
        return false;
    }
    if(!listIterator->last){
        return false;
    }

    ListNode *next = listIterator->node->next;
    free(listIterator->node);
    listIterator->last->next = next;
    listIterator->node = next;

    listIterator->list->length--;
    listIterator->list->firstNode = listIterator->dummyNode->next;

    if(next == 0){
        if(listIterator->last == listIterator->dummyNode){
            listIterator->list->lastNode = 0;
        }
        else{
            listIterator->list->lastNode = listIterator->last;
        }
    }

    return true;
}

static bool iterator_hasNext(const Iterator *iterator){
    ListIterator *listIterator = iterator->data;
    if(listIterator->list->modifications != listIterator->modificationsOnCreation){
        return false;
    }

    return (listIterator->node->next ? true : false);
}

static bool iterator_advance(const Iterator *iterator){
    ListIterator *listIterator = iterator->data;
    if(listIterator->list->modifications != listIterator->modificationsOnCreation){
        return false;
    }
    if(!iterator_hasNext(iterator)){
        return false;
    }

    listIterator->last = listIterator->node;
    listIterator->node = listIterator->node->next;
}

#include "list.h"
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
}LinkedList;

static void add(const struct List *list, void *val);
static bool removeAt(const struct List *list, unsigned int index);
static bool remove(const struct List *list, void *val);
static int length(const struct List *list);
static void* get(const struct List *list, unsigned int index);
static bool contains(const struct List *list, void* entry);
static void freeList(struct List *list);

List *list_newLinkedList(bool (*equals)(void *val1, void *val2)){
    LinkedList *list = malloc(sizeof(LinkedList));
    *list = (LinkedList){
        .firstNode = 0,
        .lastNode = 0,
        .length = 0,
        .equals = equals,
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
        .free = freeList
    };
    return result;
}

static void add(const struct List *list, void *value){
    LinkedList *linkedList = list->data;
    linkedList->length++;

    ListNode *newNode = malloc(sizeof(ListNode));
    *newNode = (ListNode){
        .value = value,
        .next = 0,
    };

    if(linkedList->lastNode == 0){
        linkedList->lastNode = newNode;
        linkedList->firstNode = newNode;
        return;
    }

    linkedList->lastNode->next = newNode;
    linkedList->lastNode = newNode;
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

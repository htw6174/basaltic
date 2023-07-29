/* Not ready for use right now, should rework to use macros or use klib generics instead */
#include <stdio.h>
#include <string.h>
#include "htw_core.h"

#define GET_ITEM_POINTER(list, index) (list->items + (index * list->itemSize))

/* Generic Lists */
typedef struct {
    u32 length;
    u32 capacity;
    size_t itemSize;
    void *items;
} List;

List *createList (size_t itemSize, u32 initialLength);
// Returns a new list object that refers to the same set of items as originalList. Resizing the new list... ?
List *sliceList(List *originalList, u32 startIndex, u32 sliceLength);
// Frees both the list object and the inner array
void destroyList(List *list);

// index must be less than list.length
void *getItem(List *list, u32 index);
// Uses memcpy to insert the value of *newItem at list[index]
void setItem(List *list, u32 index, void *newItem);
// Returns the index of the newly added item. Will expand the list if there is no available capacity.
u32 pushItem(List *list, void *newItem);
// Returns a pointer to the last item in the list, then shrinks the list. The caller is responsible for using the return value before assigning something else to the end of the list.
void *popItem(List *list);

void printList(List *list, void(*print)(void*));

List *createList(size_t itemSize, u32 initialLength) {
    void *items = malloc(itemSize * initialLength);
    // malloc l instead of using struct assignment to avoid returning the address of a local variable
    List *l = (List*)malloc(sizeof(List));
    l->items = items;
    l->length = initialLength;
    l->capacity = initialLength;
    l->itemSize = itemSize;
    return l;
}

// TODO: do something to indicate an error when out of range
List *sliceList(List *originalList, u32 startIndex, u32 sliceLength) {
    if (startIndex >= originalList->length || startIndex + sliceLength > originalList->length) {
        return NULL;
    }
    List *l = (List*)malloc(sizeof(List));
    void *newStart = getItem(originalList, startIndex);
    l->items = newStart;
    l->length = sliceLength;
    l->itemSize = originalList->itemSize;
    return l;
}

// Returns new capacity of the list
static int addCapacity(List *list) {
    int newCapacity = list->capacity * 2;
    if (newCapacity == 0)
        newCapacity = 1;
    list->items = realloc(list->items, newCapacity * list->itemSize);
    list->capacity = newCapacity;
    return newCapacity;
}

void destroyList(List *list) {
    free(list->items);
    free(list);
}

// TODO: add out of range check
// return a pointer to the item at the given index
void *getItem(List *list, u32 index)
{
    return GET_ITEM_POINTER(list, index);
}

// Uses memcpy to add the value of *newItem to list[index]
void setItem(List *list, u32 index, void *newItem) {
    memcpy(GET_ITEM_POINTER(list, index), newItem, list->itemSize);
}

// Returns the index of the newly added item
u32 pushItem(List *list, void *newItem)
{
    if (list->length == list->capacity)
        addCapacity(list);
    memcpy(GET_ITEM_POINTER(list, list->length), newItem, list->itemSize);
    list->length++;
    return list->length - 1;
}

// Returns a pointer to the last item in the list, then shrinks the list. The caller is responsible for using the return value before assigning something else to the end of the list.
void *popItem(List *list) {
    void *popped = getItem(list, list->length - 1);
    list->length--;
    return popped;
}

void printList(List *list, void(*print)(void*)) {
    printf("[");
    print(getItem(list, 0));
    for(int i = 1; i < list->length; i++) {
        printf(", ");
        print(getItem(list, i));
    }
    printf("]\n");
}

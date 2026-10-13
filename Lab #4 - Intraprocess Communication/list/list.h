#ifndef LIST_H
#define LIST_H

// list/list.h
//
// Interface definition for linked list.
//
// Giliad Dawite

#include <stdbool.h>

/* Defines the type of the elements in the linked list. */
typedef int elem;

/* Each node contains its element and points to the next node.
 * The last node's next pointer is NULL. */
struct node {
    elem value;
    struct node *next;
};
typedef struct node node_t;

/* The list simply points to the first node. */
struct list {
    node_t *head;
};
typedef struct list list_t;

/* Allocating and freeing lists. */
list_t *list_alloc();
void list_free(list_t *l);

/* Prints the list in some format. */
void list_print(list_t *l);

/* Returns string of the list (malloc'd, caller must free). */
char* listToString(list_t *l);

/* Returns a node from the heap. */
node_t *getNode(elem value);

/* Returns the length of the list. */
int list_length(list_t *l);

/* Methods for adding to the list. */
void list_add_to_back(list_t *l, elem value);
void list_add_to_front(list_t *l, elem value);
void list_add_at_index(list_t *l, elem value, int index);

/* Methods for removing from the list. Returns the removed element. */
elem list_remove_from_back(list_t *l);
elem list_remove_from_front(list_t *l);
elem list_remove_at_index(list_t *l, int index);

/* Checks to see if the given element exists in the list. */
bool list_is_in(list_t *l, elem value);

/* Returns the element at the given index. Starting index is 1. Returns -1 on error. */
elem list_get_elem_at(list_t *l, int index);

/* Returns the index at which the given element appears. Returns -1 if not found. */
int list_get_index_of(list_t *l, elem value);

#endif
// list/list.c
// 
// Implementation for linked list.
//
// <Author>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"

list_t *list_alloc() {
  list_t *mylist = (list_t *) malloc(sizeof(list_t));
  mylist->head = NULL;
  return mylist;
}

void list_free(list_t *l) {
  if (l == NULL) return;

  node_t *curr = l->head;
  while (curr != NULL) {
    node_t *next = curr->next;
    free(curr);
    curr = next;
  }
  free(l);
}

void list_print(list_t *l) {
  char *str = listToString(l);
  printf("%s\n", str);
  free(str);
}

char * listToString(list_t *l) {
  int len = list_length(l);
  // Worst case per node: "-2147483648->" = 13 chars. Plus "NULL\0" = 5.
  int buf_size = (len * 13) + 5;
  char *buf = (char *) malloc(sizeof(char) * buf_size);
  char *p = buf;

  node_t *curr = l->head;
  while (curr != NULL) {
    p += sprintf(p, "%d->", curr->value);
    curr = curr->next;
  }
  sprintf(p, "NULL");
  return buf;
}

int list_length(list_t *l) {
  int count = 0;
  node_t *curr = l->head;
  while (curr != NULL) {
    count++;
    curr = curr->next;
  }
  return count;
}

void list_add_to_back(list_t *l, elem value) {
  node_t *new_node = getNode(value);

  if (l->head == NULL) {
    l->head = new_node;
    return;
  }

  node_t *curr = l->head;
  while (curr->next != NULL) {
    curr = curr->next;
  }
  curr->next = new_node;
}
void list_add_to_front(list_t *l, elem value) {
  node_t *cur_node = getNode(value);
  cur_node->next = l->head;
  l->head = cur_node;
}

node_t * getNode(elem value) {
  node_t *mynode;

  mynode = (node_t *) malloc(sizeof(node_t));
  mynode->value = value;
  mynode->next = NULL;

  return mynode;
}

void list_add_at_index(list_t *l, elem value, int index) {
  // 1-indexed, matching list_get_elem_at. Index <= 1 (or empty list)
  // inserts at front; index past the end just appends.
  if (index <= 1 || l->head == NULL) {
    list_add_to_front(l, value);
    return;
  }

  node_t *curr = l->head;
  int count = 1;
  while (curr->next != NULL && count < index - 1) {
    curr = curr->next;
    count++;
  }

  node_t *new_node = getNode(value);
  new_node->next = curr->next;
  curr->next = new_node;
}

elem list_remove_from_back(list_t *l) {
  if (l->head == NULL) return -1;

  if (l->head->next == NULL) {
    elem val = l->head->value;
    free(l->head);
    l->head = NULL;
    return val;
  }

  node_t *curr = l->head;
  while (curr->next->next != NULL) {
    curr = curr->next;
  }
  elem val = curr->next->value;
  free(curr->next);
  curr->next = NULL;
  return val;
}

elem list_remove_from_front(list_t *l) {
  if (l->head == NULL) return -1;

  node_t *old_head = l->head;
  elem val = old_head->value;
  l->head = old_head->next;
  free(old_head);
  return val;
}

elem list_remove_at_index(list_t *l, int index) {
  if (l->head == NULL || index < 1) return -1;

  if (index == 1) {
    return list_remove_from_front(l);
  }

  node_t *curr = l->head;
  int count = 1;
  while (curr != NULL && count < index - 1) {
    curr = curr->next;
    count++;
  }

if (curr == NULL || curr->next == NULL) return -1;

  node_t *to_remove = curr->next;
  elem val = to_remove->value;
  curr->next = to_remove->next;
  free(to_remove);
  return val;
}

bool list_is_in(list_t *l, elem value) {
  node_t *curr = l->head;
  while (curr != NULL) {
    if (curr->value == value) return true;
    curr = curr->next;
  }
  return false;
}

elem list_get_elem_at(list_t *l, int index) {
  if (index < 1) return -1;

  node_t *curr = l->head;
  int count = 1;
  while (curr != NULL && count < index) {
    curr = curr->next;
    count++;
  }

  if (curr == NULL) return -1;
  return curr->value;
}

int list_get_index_of(list_t *l, elem value) {
  node_t *curr = l->head;
  int index = 1;
  while (curr != NULL) {
    if (curr->value == value) return index;
    curr = curr->next;
    index++;
  }
  return -1;
}




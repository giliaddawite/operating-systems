#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"

list_t *list_alloc() {
    list_t *l = malloc(sizeof(list_t));
    if (l) l->head = NULL;
    return l;
}

void list_free(list_t *l) {
    if (!l) return;
    node_t *cur = l->head;
    while (cur) {
        node_t *next = cur->next;
        free(cur);
        cur = next;
    }
    free(l);
}

node_t *getNode(elem value) {
    node_t *n = malloc(sizeof(node_t));
    if (n) { n->value = value; n->next = NULL; }
    return n;
}

int list_length(list_t *l) {
    int len = 0;
    for (node_t *c = l->head; c; c = c->next) len++;
    return len;
}

char *listToString(list_t *l) {
    int len = list_length(l);
    char *s = malloc(16 * (len + 1) + 8);
    if (!s) return NULL;
    s[0] = '\0';
    char tmp[16];
    for (node_t *c = l->head; c; c = c->next) {
        sprintf(tmp, "%d->", c->value);
        strcat(s, tmp);
    }
    strcat(s, "NULL");
    return s;
}

void list_print(list_t *l) {
    char *s = listToString(l);
    if (s) { printf("%s\n", s); free(s); }
}

void list_add_to_front(list_t *l, elem value) {
    node_t *n = getNode(value);
    if (!n) return;
    n->next = l->head;
    l->head = n;
}

void list_add_to_back(list_t *l, elem value) {
    node_t *n = getNode(value);
    if (!n) return;
    if (!l->head) { l->head = n; return; }
    node_t *c = l->head;
    while (c->next) c = c->next;
    c->next = n;
}

void list_add_at_index(list_t *l, elem value, int index) {
    if (index < 1 || index > list_length(l) + 1) return;
    if (index == 1) { list_add_to_front(l, value); return; }
    node_t *prev = l->head;
    for (int i = 1; i < index - 1; i++) prev = prev->next;
    node_t *n = getNode(value);
    if (!n) return;
    n->next = prev->next;
    prev->next = n;
}

elem list_remove_from_front(list_t *l) {
    if (!l->head) return -1;
    node_t *n = l->head;
    elem v = n->value;
    l->head = n->next;
    free(n);
    return v;
}

elem list_remove_from_back(list_t *l) {
    if (!l->head) return -1;
    if (!l->head->next) return list_remove_from_front(l);
    node_t *prev = l->head;
    while (prev->next->next) prev = prev->next;
    elem v = prev->next->value;
    free(prev->next);
    prev->next = NULL;
    return v;
}

elem list_remove_at_index(list_t *l, int index) {
    if (index < 1 || index > list_length(l)) return -1;
    if (index == 1) return list_remove_from_front(l);
    node_t *prev = l->head;
    for (int i = 1; i < index - 1; i++) prev = prev->next;
    node_t *n = prev->next;
    elem v = n->value;
    prev->next = n->next;
    free(n);
    return v;
}

bool list_is_in(list_t *l, elem value) {
    for (node_t *c = l->head; c; c = c->next)
        if (c->value == value) return true;
    return false;
}

elem list_get_elem_at(list_t *l, int index) {
    if (index < 1) return -1;
    node_t *c = l->head;
    for (int i = 1; c && i < index; i++) c = c->next;
    return c ? c->value : -1;
}

int list_get_index_of(list_t *l, elem value) {
    int i = 1;
    for (node_t *c = l->head; c; c = c->next, i++)
        if (c->value == value) return i;
    return -1;
}
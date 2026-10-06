/*
 * StudentOS — Dynamic string array implementation
 * Week 2 (M1·CO1): Dynamic string / vector types
 *
 * StringVector: a heap-allocated, automatically-resizing array of
 * null-terminated C strings. Used as the primary token container.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vector.h"

#define VECTOR_INIT_CAPACITY 8

StringVector *vector_create(void) {
    StringVector *v = malloc(sizeof(StringVector));
    if (!v) return NULL;

    v->items = malloc(VECTOR_INIT_CAPACITY * sizeof(char *));
    if (!v->items) { free(v); return NULL; }

    v->size     = 0;
    v->capacity = VECTOR_INIT_CAPACITY;
    return v;
}

int vector_push(StringVector *v, const char *item) {
    if (!v || !item) return -1;

    /* Grow by doubling when full */
    if (v->size >= v->capacity) {
        size_t new_cap   = v->capacity * 2;
        char **new_items = realloc(v->items, new_cap * sizeof(char *));
        if (!new_items) return -1;
        v->items    = new_items;
        v->capacity = new_cap;
    }

    v->items[v->size] = strdup(item);
    if (!v->items[v->size]) return -1;
    v->size++;
    return 0;
}

void vector_free(StringVector *v) {
    if (!v) return;
    for (size_t i = 0; i < v->size; i++) free(v->items[i]);
    free(v->items);
    free(v);
}

#ifndef VECTOR_H
#define VECTOR_H

/*
 * StudentOS — Dynamic string array
 * Week 2 (M1·CO1): Dynamic string / vector types
 *
 * StringVector is a heap-allocated, resizable array of C strings.
 * It is the primary data structure for the shell tokenizer output.
 */

#include <stddef.h>

typedef struct {
    char   **items;    /* Heap-allocated array of heap-allocated strings */
    size_t   size;     /* Number of items currently stored               */
    size_t   capacity; /* Allocated slots (doubles on overflow)          */
} StringVector;

/* Allocate and initialise a new empty StringVector. Returns NULL on OOM. */
StringVector *vector_create(void);

/* Append a copy of 'item' to the vector. Returns 0 on success, -1 on OOM. */
int vector_push(StringVector *v, const char *item);

/* Free every string in the vector, the items array, and the vector itself. */
void vector_free(StringVector *v);

#endif /* VECTOR_H */

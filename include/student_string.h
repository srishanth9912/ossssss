#ifndef STUDENT_STRING_H
#define STUDENT_STRING_H

#include <stddef.h>

/*
 * Dynamic string type (ShellForge Week 2 Milestone)
 */
typedef struct
{
    char *data;
    size_t length;
    size_t capacity;
} DynamicString;

/* Create and initialize a new DynamicString */
DynamicString *string_create(void);

/* Append a C-string to the dynamic string */
int string_append(DynamicString *string, const char *text);

/* Append a single character to the dynamic string */
int string_append_c(DynamicString *string, char c);

/* Free all memory allocated by the dynamic string */
void string_free(DynamicString *string);

#endif /* STUDENT_STRING_H */


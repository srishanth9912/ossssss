#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "student_string.h"

#define INITIAL_STRING_CAPACITY 16

DynamicString *string_create(void)
{
    DynamicString *string = malloc(sizeof(DynamicString));

    if (string == NULL)
    {
        return NULL;
    }

    string->data = malloc(INITIAL_STRING_CAPACITY);

    if (string->data == NULL)
    {
        free(string);
        return NULL;
    }

    string->length = 0;
    string->capacity = INITIAL_STRING_CAPACITY;
    string->data[0] = '\0';

    return string;
}

int string_append(DynamicString *string, const char *text)
{
    if (string == NULL || text == NULL)
    {
        return -1;
    }

    size_t text_length = strlen(text);
    size_t required_capacity = string->length + text_length + 1;

    while (string->capacity < required_capacity)
    {
        string->capacity *= 2;
    }

    char *new_data = realloc(string->data, string->capacity);

    if (new_data == NULL)
    {
        return -1;
    }

    string->data = new_data;
    memcpy(string->data + string->length, text, text_length + 1);
    string->length += text_length;

    return 0;
}

int string_append_c(DynamicString *string, char c)
{
    char tmp[2] = {c, '\0'};
    return string_append(string, tmp);
}

void string_free(DynamicString *string)
{
    if (string == NULL)
    {
        return;
    }

    free(string->data);
    free(string);
}


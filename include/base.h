#ifndef INCLUDE_BASE_H
#define INCLUDE_BASE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char* ptr;
    size_t size;
} string_view;

string_view sv_make(const char* str, size_t size);
void sv_print(string_view sv);
string_view sv_slice(string_view sv, size_t start, size_t len);
string_view sv_slice_end(string_view sv, size_t start);
bool sv_starts_with(string_view sv, const char* prefix);
bool sv_starts_with_sv(string_view sv, string_view prefix);
bool sv_eq(string_view a, string_view b);
bool sv_eq_cstr(string_view sv, const char* str);

typedef struct {
    char* items;
    size_t len;
    size_t cap;
} string_builder;

string_builder sb_new(size_t cap);
void sb_destroy(string_builder* sb);
void sb_push_cstr(string_builder* sb, const char* str);
char* sb_to_cstr(string_builder* sb);

// DYNAMIC ARRAYS
#define DA_DECLARE(name, t)                                                                        \
    typedef struct {                                                                               \
        t* items;                                                                                  \
        size_t len;                                                                                \
        size_t cap;                                                                                \
    } name

#define DA_INIT(arr, l, c)                                                                         \
    do {                                                                                           \
        (arr)->items = (__typeof__((arr)->items[0])*)malloc(sizeof((arr)->items[0]) * c);          \
        (arr)->cap = c;                                                                            \
        (arr)->len = l;                                                                            \
        memset((arr)->items, 0, l * sizeof((arr)->items[0]));                                      \
    } while (0)

#define DA_RESIZE(arr, c)                                                                          \
    do {                                                                                           \
        (arr)->cap = c;                                                                            \
        (arr)->items = realloc((arr)->items, c * sizeof((arr)->items[0]));                         \
    } while (0)

#define DA_GROW(arr)                                                                               \
    do {                                                                                           \
        (arr)->cap *= 2;                                                                           \
        DA_RESIZE((arr), (arr)->cap);                                                              \
    } while (0)

#define DA_ADD(arr, elem)                                                                          \
    do {                                                                                           \
        if ((arr)->len == (arr)->cap) {                                                            \
            DA_GROW(arr);                                                                          \
        }                                                                                          \
        (arr)->items[(arr)->len++] = elem;                                                         \
    } while (0)

#define DA_FREE(arr)                                                                               \
    do {                                                                                           \
        free((arr)->items);                                                                        \
    } while (0)

#ifdef BASE_IMPL

#include <stdio.h>

// STRING VIEW
string_view sv_make(const char* str, size_t size)
{
    return (string_view) { .ptr = str, .size = size };
}

void sv_print(string_view sv)
{
    printf("%.*s", (int)sv.size, sv.ptr);
}

string_view sv_slice(string_view sv, size_t start, size_t len)
{
    return (string_view) { .ptr = sv.ptr + start, .size = len };
}

string_view sv_slice_end(string_view sv, size_t start)
{
    return sv_slice(sv, start, sv.size - start);
}

bool sv_starts_with(string_view sv, const char* prefix)
{
    size_t len = strlen(prefix);
    for (size_t i = 0; i < sv.size && i < len; i++) {
        if (sv.ptr[i] != prefix[i]) {
            return false;
        }
    }

    return true;
}

bool sv_starts_with_sv(string_view sv, string_view prefix)
{
    for (size_t i = 0; i < sv.size && i < prefix.size; i++) {
        if (sv.ptr[i] != prefix.ptr[i]) {
            return false;
        }
    }

    return true;
}

bool sv_eq(string_view a, string_view b)
{
    return strncmp(a.ptr, b.ptr, a.size) == 0;
}

bool sv_eq_cstr(string_view sv, const char* str)
{
    return strncmp(sv.ptr, str, sv.size) == 0;
}

// STRING BUILDER
string_builder sb_new(size_t cap)
{
    string_builder sb;
    DA_INIT(&sb, 0, cap);
    return sb;
}

void sb_push_cstr(string_builder* sb, const char* str)
{
    while (*str) {
        DA_ADD(sb, *str++);
    }
}

char* sb_to_cstr(string_builder* sb)
{
    DA_ADD(sb, '\0');
    return sb->items;
}

void sb_destroy(string_builder* sb)
{
    DA_FREE(sb);
}

#endif
#endif

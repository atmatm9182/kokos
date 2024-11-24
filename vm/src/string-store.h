#ifndef STRING_STORE_H_
#define STRING_STORE_H_

#include "runtime.h"

typedef struct {
    const kokos_runtime_string_t** items;
    // use longer field names so the struct can't be used as a dynamic array
    size_t length;
    size_t capacity;
} kokos_string_store_t;

void kokos_string_store_init(kokos_string_store_t* store, size_t cap);
void kokos_string_store_destroy(kokos_string_store_t*);

const kokos_runtime_string_t* kokos_string_store_add(
    kokos_string_store_t* store, const kokos_runtime_string_t* string);
const kokos_runtime_string_t* kokos_string_store_add_sv(
    kokos_string_store_t* store, string_view sv);
const kokos_runtime_string_t* kokos_string_store_add_cstr(
    kokos_string_store_t* store, const char* cstr);
const kokos_runtime_string_t* kokos_string_store_find(
    const kokos_string_store_t* store, string_view key);

#endif // STRING_STORE_H_

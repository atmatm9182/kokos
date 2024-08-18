#ifndef STRING_STORE_H_
#define STRING_STORE_H_

#include "runtime.h"

typedef struct {
    kokos_runtime_string_t const** items;
    // use longer field names so the struct can't be used as a dynamic array
    size_t length;
    size_t capacity;
} kokos_string_store_t;

void kokos_string_store_init(kokos_string_store_t* store, size_t cap);
kokos_runtime_string_t const* kokos_string_store_add(
    kokos_string_store_t* store, kokos_runtime_string_t const* string);
kokos_runtime_string_t const* kokos_string_store_add_cstr(
    kokos_string_store_t* store, char const* cstr);
kokos_runtime_string_t const* kokos_string_store_find(
    kokos_string_store_t const* store, string_view key);

#endif // STRING_STORE_H_

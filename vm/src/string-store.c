#include "string-store.h"
#include "hash.h"
#include "macros.h"
#include "src/runtime.h"
#include <string.h>

void kokos_string_store_init(kokos_string_store_t* store, size_t cap)
{
    store->items = KOKOS_CALLOC(sizeof(kokos_runtime_string_t*), cap);
    store->length = 0;
    store->capacity = cap;
}

void kokos_string_store_destroy(kokos_string_store_t* store)
{
    for (size_t i = 0; i < store->capacity; i++) {
        kokos_runtime_string_t* str = (void*)store->items[i];
        if (!str) {
            continue;
        }

        kokos_runtime_string_destroy((kokos_runtime_string_t*)store->items[i]);
    }

    KOKOS_FREE(store->items);
}

static inline float kokos_string_store_load(kokos_string_store_t const* store)
{
    return (float)store->length / (float)store->capacity * 100;
}

static inline void kokos_string_store_grow(kokos_string_store_t* store)
{
    kokos_string_store_t new_store;
    kokos_string_store_init(&new_store, store->capacity * 2);

    for (size_t i = 0; i < store->capacity; i++) {
        kokos_runtime_string_t const* str = store->items[i];
        if (!str)
            continue;

        kokos_string_store_add(&new_store, str);
    }

    KOKOS_FREE(store->items);

    *store = new_store;
}

kokos_runtime_string_t const* kokos_string_store_add(
    kokos_string_store_t* store, kokos_runtime_string_t const* string)
{
    if (kokos_string_store_load(store) >= 70) {
        kokos_string_store_grow(store);
    }

    uint64_t idx = hash_djb2_len(string->ptr, string->len) % store->capacity;
    kokos_runtime_string_t const* cur;

    while ((cur = store->items[idx])) {
        if (cur->len == string->len && strncmp(cur->ptr, string->ptr, cur->len) == 0) {
            return cur; // our set already contains this string
        }

        idx = (idx + 1) % store->capacity;
    }

    store->items[idx] = string;
    store->length++;
    return string;
}

kokos_runtime_string_t const* kokos_string_store_add_sv(kokos_string_store_t* store, string_view sv)
{
    if (kokos_string_store_load(store) >= 70) {
        kokos_string_store_grow(store);
    }

    uint64_t idx = hash_djb2_len(sv.ptr, sv.size) % store->capacity;
    kokos_runtime_string_t const* cur;

    while ((cur = store->items[idx])) {
        if (cur->len == sv.size && strncmp(cur->ptr, sv.ptr, cur->len) == 0) {
            return cur; // our set already contains this string
        }

        idx = (idx + 1) % store->capacity;
    }

    store->items[idx] = kokos_runtime_string_from_sv(sv);
    store->length++;
    return store->items[idx];
}

kokos_runtime_string_t const* kokos_string_store_add_cstr(
    kokos_string_store_t* store, char const* cstr)
{
    if (kokos_string_store_load(store) >= 70) {
        kokos_string_store_grow(store);
    }

    size_t len = strlen(cstr);

    uint64_t idx = hash_djb2_len(cstr, len) % store->capacity;
    kokos_runtime_string_t const* cur;

    while ((cur = store->items[idx])) {
        if (cur->len == len && strncmp(cur->ptr, cstr, cur->len) == 0) {
            return cur; // our set already contains this string
        }

        idx = (idx + 1) % store->capacity;
    }

    store->items[idx] = kokos_runtime_string_new(cstr, len);
    store->length++;
    return store->items[idx];
}

static inline bool runtime_string_eq_string_view(
    kokos_runtime_string_t const* string, string_view sv)
{
    if (string->len != sv.size) {
        return false;
    }

    for (size_t i = 0; i < sv.size; i++) {
        if (string->ptr[i] != sv.ptr[i]) {
            return false;
        }
    }

    return true;
}

kokos_runtime_string_t const* kokos_string_store_find(
    kokos_string_store_t const* store, string_view key)
{
    uint64_t idx = hash_djb2_len(key.ptr, key.size) % store->capacity;

    while (store->items[idx]) {
        if (runtime_string_eq_string_view(store->items[idx], key)) {
            return store->items[idx];
        }

        idx = (idx + 1) % store->capacity;
    }

    return NULL;
}

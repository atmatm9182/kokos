#include "runtime.h"
#include "base.h"
#include "hash.h"
#include "macros.h"
#include "string.h"
#include "value.h"
#include <stdio.h>

uint64_t kokos_djb2(void const* ptr)
{
    uint64_t value = (uint64_t)ptr;
    switch (GET_TAG(value)) {
    case STRING_TAG: {
        kokos_runtime_string_t* string = (kokos_runtime_string_t*)(value & ~STRING_BITS);
        return hash_djb2_len(string->ptr, string->len);
    }
    default: KOKOS_TODO();
    }
}

bool kokos_eq(void const* lhs, void const* rhs)
{
    kokos_value_t l = { .as_int = (uintptr_t)lhs };
    kokos_value_t r = { .as_int = (uintptr_t)rhs };

    if (IS_DOUBLE(l) && IS_DOUBLE(r)) {
        return lhs == rhs;
    }

    uint16_t ltag = VALUE_TAG(l);
    uint16_t rtag = VALUE_TAG(r);

    if (ltag != rtag) {
        return false;
    }

    switch (VALUE_TAG(l)) {
    case STRING_TAG: {
        kokos_runtime_string_t* ls = GET_STRING(l);
        kokos_runtime_string_t* rs = GET_STRING(r);

        if (ls->len != rs->len) {
            return false;
        }

        for (size_t i = 0; i < ls->len; i++) {
            if (ls->ptr[i] != rs->ptr[i]) {
                return false;
            }
        }

        return true;
    }
    case VECTOR_TAG: {
        kokos_runtime_vector_t* lv = GET_VECTOR(l);
        kokos_runtime_vector_t* rv = GET_VECTOR(r);

        if (lv->len != rv->len) {
            return false;
        }

        for (size_t i = 0; i < lv->len; i++) {
            void* lhs = TO_PTR(lv->items[i]);
            void* rhs = TO_PTR(rv->items[i]);

            if (!kokos_eq(lhs, rhs)) {
                return false;
            }
        }

        return true;
    }
    case MAP_TAG: {
        return lhs == rhs; // should we somehow check for equality of all keys?
    }
    default: {
        char buf[512];
        sprintf(buf, "kokos_eq not implemented for values with tag %lx", VALUE_TAG(l));
        KOKOS_TODO(buf);
    }
    }
}

ht_eq_func kokos_default_map_eq_func = kokos_eq;

ht_hash_func kokos_default_map_hash_func = kokos_djb2;

void kokos_runtime_map_add(kokos_runtime_map_t* map, kokos_value_t key, kokos_value_t value)
{
    ht_add(&map->table, (void*)key.as_int, (void*)value.as_int);
}

kokos_runtime_string_t* kokos_runtime_string_new(char const* data, size_t len)
{
    kokos_runtime_string_t* string = KOKOS_ALLOC(sizeof(kokos_runtime_string_t));
    string->ptr = KOKOS_CALLOC(len + 1, sizeof(char));
    string->len = len;
    memcpy(string->ptr, data, string->len);
    return string;
}

#include "runtime.h"
#include "base.h"
#include "compile.h"
#include "hash.h"
#include "macros.h"
#include "value.h"

uint64_t kokos_djb2(const void* ptr)
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

bool kokos_eq(const void* lhs, const void* rhs)
{
    // FIXME: define equality for collection types
    return lhs == rhs;
}

ht_eq_func kokos_default_map_eq_func = kokos_eq;

ht_hash_func kokos_default_map_hash_func = kokos_djb2;

void kokos_runtime_map_add(kokos_runtime_map_t* map, kokos_value_t key, kokos_value_t value)
{
    ht_add(&map->table, (void*)key.as_int, (void*)value.as_int);
}

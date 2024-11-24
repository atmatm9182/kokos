#include "hash.h"
#include "runtime.h"

#include <string.h>

uint64_t hash_djb2_len(const char* str, size_t len)
{
    int64_t hash = 5381;

    for (size_t i = 0; i < len; i++) {
        int c = str[i];
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

uint64_t hash_djb2(const char* str)
{
    int64_t hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

uint64_t hash_cstring_func(const void* ptr)
{
    return hash_djb2((const char*)ptr);
}

bool hash_cstring_eq_func(const void* lhs, const void* rhs)
{
    const char* ls = (const char*)lhs;
    const char* rs = (const char*)rhs;

    return strcmp(ls, rs) == 0;
}

uint64_t hash_sizet_func(const void* ptr)
{
    return (size_t)ptr;
}

bool hash_sizet_eq_func(const void* lhs, const void* rhs)
{
    return (size_t)lhs == (size_t)rhs;
}

uint64_t hash_runtime_string_func(const void* ptr)
{
    const kokos_runtime_string_t* string = ptr;
    return hash_djb2_len(string->ptr, string->len);
}

bool hash_runtime_string_eq_func(const void* lhs, const void* rhs)
{
    const kokos_runtime_string_t* l = lhs;
    const kokos_runtime_string_t* r = rhs;

    if (l->len != r->len) {
        return false;
    }

    return strncmp(l->ptr, r->ptr, l->len) == 0;
}
